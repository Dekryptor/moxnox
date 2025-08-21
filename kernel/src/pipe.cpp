module;

#include <span>
#include <optional>
#include <atomic>
#include <string_view>
import objects;
#include "process.h"
import event;
#include "thread.h"
#include "spinlock.h"
#include "scheduler.h"

export module pipe;

import print;
import mem;
import config;
import list;
import bsod;
import chardev;
import unicode;

using namespace std;

export class pipe : public chardev {
public:
    pipe(unsigned int buffer_pages);
    virtual ~pipe();

    NTSTATUS read(span<uint8_t> s, uint64_t& read_bytes) override;

    NTSTATUS write(span<const uint8_t> s, uint64_t& written_bytes) override;

    NTSTATUS write(string_view sv, uint64_t& written_bytes) {
        return write(span((uint8_t*)sv.data(), sv.length()), written_bytes);
    }

    void clear() override;

    uint8_t* buf;
    unsigned int buf_pages;
    size_t buf_unread = 0;
    size_t buf_write_pos = 0;
    bool server_closed = false;
    spinlock lock;
    event not_full_event;
    list_entry flushable_le;
};

class named_duplex_pipe_client : public chardev {
public:
    named_duplex_pipe_client(pipe& recv_pipe, pipe& send_pipe) : recv_pipe(recv_pipe), send_pipe(send_pipe) {
        recv_pipe.acquire();
        send_pipe.acquire();
    }

    ~named_duplex_pipe_client() {
        recv_pipe.release();
        send_pipe.release();
    }

    nt_status read(span<uint8_t> s, uint64_t& read_bytes) override {
        auto ret = recv_pipe.read(s, read_bytes);

        if (disconnected && read_bytes == 0) {
            lock_guard l(recv_pipe.lock);

            if (recv_pipe.buf_unread == 0)
                return nt_status::PIPE_DISCONNECTED;
        }

        return ret;
    }

    nt_status write(span<const uint8_t> s, uint64_t& written_bytes) override {
        if (disconnected)
            return nt_status::PIPE_DISCONNECTED;

        auto ret = send_pipe.write(s, written_bytes);

        if (disconnected)
            return nt_status::PIPE_DISCONNECTED;

        return ret;
    }

    sync_object& get_sync_object() override {
        return recv_pipe;
    }

    void server_disconnected() {
        sync_lock sl;
        lock_guard l(sl);

        disconnected = true;

        send_pipe.readonly = true;

        if (!send_pipe.not_full_event.signalled)
            send_pipe.not_full_event.set_holding_sync_lock();

        if (!recv_pipe.signalled) {
            recv_pipe.signalled = true;

            wake_up_waiters(recv_pipe.waiters);
        }
    }

    pipe& recv_pipe;
    pipe& send_pipe;
    bool disconnected = false;
};

class named_duplex_pipe_server : public chardev {
public:
    named_duplex_pipe_server(named_duplex_pipe_client& client) : client(client) {
        client.acquire();
    }

    ~named_duplex_pipe_server() {
        client.release();
    }

    nt_status read(span<uint8_t> s, uint64_t& read_bytes) override {
        return client.send_pipe.read(s, read_bytes);
    }

    nt_status write(span<const uint8_t> s, uint64_t& written_bytes) override {
        return client.recv_pipe.write(s, written_bytes);
    }

    sync_object& get_sync_object() override {
        return client.send_pipe;
    }

    void last_handle_closed() override {
        client.server_disconnected();
    }

    named_duplex_pipe_client& client;
};

static spinlock flushable_pipes_lock;
static list_entry flushable_pipes = { &flushable_pipes, &flushable_pipes };

pipe::pipe(unsigned int buffer_pages) : buf_pages(buffer_pages) {
    signalled = false;
    not_full_event.autoreset = false;
    not_full_event.signalled = true;

    if (buffer_pages == 0) {
        buf = nullptr;
        return;
    }

    auto va = allocate_va(buffer_pages);
    if (!va) {
        // FIXME - exception
        print("allocate_va failed\n");
        return;
    }

    for (unsigned int i = 0; i < buffer_pages; i++) {
        auto pa = allocate_page();
        if (!pa) {
            // FIXME - exception
            print("allocate_page failed\n");

            for (unsigned int j = 0; j < i; j++) {
                auto pa = va_to_pa_lookup(va + (j << PAGE_SHIFT));

                free_folio(pa);
                unmap_page(va + (j << PAGE_SHIFT));
            }

            free_va(va, buffer_pages);
            return;
        }

        if (!map_page(pa, va + (i << PAGE_SHIFT), mapping_type::read_write)) {
            // FIXME - exception
            print("map_page failed\n");

            for (unsigned int j = 0; j < i; j++) {
                auto pa = va_to_pa_lookup(va + (j << PAGE_SHIFT));

                free_folio(pa);
                unmap_page(va + (j << PAGE_SHIFT));
            }

            free_folio(pa);
            free_va(va, buffer_pages);
            return;
        }
    }

    buf = (uint8_t*)va;
}

pipe::~pipe() {
    if (!buf)
        return;

    auto va = (uintptr_t)buf;

    for (unsigned int i = 0; i < buf_pages; i++) {
        auto pa = va_to_pa_lookup(va);

        free_folio(pa);
        unmap_page(va);

        va += PAGE_SIZE;
    }

    free_va((uintptr_t)buf, buf_pages);
}

nt_status pipe::write(span<const uint8_t> s, uint64_t& written_bytes) {
    auto orig_s = s;

    if (readonly) {
        written_bytes = 0;
        return nt_status::MEDIA_WRITE_PROTECTED;
    }

    if (server_closed) {
        written_bytes = 0;
        return nt_status::PIPE_DISCONNECTED;
    }

    if (s.empty()) {
        written_bytes = 0;
        return nt_status::SUCCESS;
    }

    do {
        size_t to_copy;

        {
            lock_guard l(lock);

            size_t buf_left = (buf_pages << PAGE_SHIFT) - buf_unread;

            to_copy = min(s.size(), buf_left);

            if (to_copy > 0) {
                if (buf_write_pos + to_copy > buf_pages << PAGE_SHIFT) {
                    auto first_bit = (buf_pages << PAGE_SHIFT) - buf_write_pos;

                    memcpy(&buf[buf_write_pos], s.data(), first_bit);

                    buf_write_pos = 0;
                    buf_unread += first_bit;
                    to_copy -= first_bit;
                    s = s.subspan(first_bit);
                }

                memcpy(&buf[buf_write_pos], s.data(), to_copy);
                buf_write_pos += to_copy;
                buf_unread += to_copy;
                s = s.subspan(to_copy);
            }
        }

        if (to_copy > 0) {
            // wake up waiting thread

            sync_lock sl;
            lock_guard l(sl);

            if (!signalled) {
                signalled = true;

                wake_up_waiters(waiters);
            }

            {
                lock_guard l2(lock);

                if (buf_unread == buf_pages << PAGE_SHIFT && !server_closed)
                    not_full_event.signalled = false;
            }
        }

        if (s.empty())
            break;

        // block while we wait for space to become available

        if (wait_for_object(not_full_event, true) == nt_status::ALERTED)
            break;

        if (server_closed)
            break;

        if (readonly)
            break;
    } while (true);

    written_bytes = (uint64_t)(s.data() - orig_s.data());

    return nt_status::SUCCESS;
}

NTSTATUS pipe::read(span<uint8_t> s, uint64_t& read_bytes) {
    size_t copied = 0;

    if (s.empty()) {
        read_bytes = 0;
        return nt_status::SUCCESS;
    }

    optional<uint64_t> timeout;

    acquire();
    object_ref obj{*this}; // so refcount gets decreased on exit

    if (server_closed)
        timeout = 0;

    auto status = wait_for_object(*this, true, timeout);

    if (status == nt_status::ALERTED) {
        read_bytes = 0;
        return nt_status::SUCCESS;
    } else if (status == nt_status::TIMEOUT) {
        read_bytes = 0;
        return nt_status::PIPE_DISCONNECTED;
    } else if (!success(status))
        return status;

    {
        sync_lock sl;
        lock_guard l(sl);
        lock_guard l2(lock);

        if (buf_unread == 0) {
            read_bytes = 0;
            return nt_status::SUCCESS;
        }

        size_t buf_read_pos = buf_write_pos + (buf_pages << PAGE_SHIFT) - buf_unread;

        while (buf_read_pos >= buf_pages << PAGE_SHIFT) {
            buf_read_pos -= buf_pages << PAGE_SHIFT;
        }

        size_t to_copy = min(s.size(), buf_unread);

        if (buf_read_pos + to_copy > buf_pages << PAGE_SHIFT) {
            auto first_bit = (buf_pages << PAGE_SHIFT) - buf_read_pos;

            memcpy(s.data(), &buf[buf_read_pos], first_bit);

            buf_read_pos = 0;
            buf_unread -= first_bit;
            to_copy -= first_bit;
            s = s.subspan(first_bit);
            copied = first_bit;
        }

        memcpy(s.data(), &buf[buf_read_pos], to_copy);
        buf_unread -= to_copy;
        copied += to_copy;

        if (buf_unread == 0)
            signalled = false;

        if (!not_full_event.signalled)
            not_full_event.set_holding_sync_lock();
    }

    read_bytes = copied;
    return nt_status::SUCCESS;
}

void pipe::clear() {
    sync_lock sl;
    lock_guard l(sl);
    lock_guard l2(lock);

    buf_write_pos = 0;
    buf_unread = 0;
    signalled = false;

    if (not_full_event.signalled) {
        not_full_event.signalled = false;
        wake_up_waiters(not_full_event.waiters);
    }
}

export void add_to_flushable_pipes_list(pipe& p) {
    lock_guard lock(flushable_pipes_lock);

    insert_tail_list(flushable_pipes, p.flushable_le);
}

export void shutdown_pipes() {
    while (true) {
        pipe* p;

        {
            lock_guard lock(flushable_pipes_lock);

            if (is_list_empty(flushable_pipes))
                return;

            p = (pipe*)((uint8_t*)flushable_pipes.next - offsetof(pipe, flushable_le));
            remove_entry_list(p->flushable_le);
        }

        p->flush(true);
    }
}

extern "C"
NTSTATUS NtCreateNamedPipeFile(HANDLE* file_handle, ACCESS_MASK desired_access,
                               const OBJECT_ATTRIBUTES* object_attributes,
                               IO_STATUS_BLOCK* iosb, uint32_t /*share_access*/,
                               uint32_t create_disposition, uint32_t /*create_options*/,
                               uint32_t /*named_pipe_type*/, uint32_t /*read_mode*/,
                               uint32_t /*completion_mode*/, uint32_t /*maximum_instances*/,
                               uint32_t /*inbound_quota*/, uint32_t /*outbound_quota*/,
                               const LARGE_INTEGER* /*default_timeout*/) {
    nt_status status;
    auto t = get_current_thread();

    // FIXME - share_access
    // FIXME - create_options
    // FIXME - named_pipe_type
    // FIXME - read_mode
    // FIXME - completion_mode
    // FIXME - maximum_instances
    // FIXME - inbound_quota
    // FIXME - outbound_quote
    // FIXME - default_timeout

    switch (create_disposition) {
        case FILE_SUPERSEDE:
            print("FIXME - FILE_SUPERSEDE\n");
            return nt_status::NOT_IMPLEMENTED;

        case FILE_OPEN:
            print("FIXME - FILE_OPEN\n");
            return nt_status::NOT_IMPLEMENTED;

        case FILE_OPEN_IF:
            print("FIXME - FILE_OPEN_IF\n");
            return nt_status::NOT_IMPLEMENTED;

        case FILE_OVERWRITE:
            print("FIXME - FILE_OVERWRITE\n");
            return nt_status::NOT_IMPLEMENTED;

        case FILE_OVERWRITE_IF:
            print("FIXME - FILE_OVERWRITE_IF\n");
            return nt_status::NOT_IMPLEMENTED;

        case FILE_CREATE:
            break;

        default:
            print("unrecognized create disposition {}\n", create_disposition);
            return nt_status::INVALID_PARAMETER;
    }

    if (!object_attributes->ObjectName || !object_attributes->ObjectName->Buffer)
        return nt_status::INVALID_PARAMETER;

    // FIXME - check desired_access

    auto u16name = u16string_view(object_attributes->ObjectName->Buffer, object_attributes->ObjectName->Length / sizeof(char16_t));

    if (u16name.empty())
        return nt_status::INVALID_PARAMETER;

    auto namelen = utf16_to_utf8_len(u16name);

    if (namelen == 0)
        return nt_status::INVALID_PARAMETER;

    auto name = (char*)malloc(namelen);
    if (!name)
        return nt_status::INSUFFICIENT_RESOURCES;

    utf16_to_utf8(span(name, namelen), u16name);

    for (unsigned int i = 0; i < namelen; i++) {
        if (name[i] == '\\')
            name[i] = '/';
    }

    // FIXME - shouldn't be in /dev?
    static const string_view dev_prefix = "/dev/";
    object_ref obj_ref;

    if (namelen < dev_prefix.size() || string_view(name, dev_prefix.size()) != dev_prefix) {
        free(name);
        return nt_status::OBJECT_NAME_NOT_FOUND;
    }

    // FIXME - pipe size
    auto pptr_recv = new pipe(1);
    if (!pptr_recv) {
        print("new failed\n");
        free(name);
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    auto obj = object_ref{*pptr_recv};

    // FIXME - pipe size
    auto pptr_send = new pipe(1);
    if (!pptr_send) {
        print("new failed\n");
        free(name);
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    auto obj2 = object_ref{*pptr_send};

    auto pptr_client = new named_duplex_pipe_client(*pptr_recv, *pptr_send);
    if (!pptr_client) {
        print("new failed\n");
        free(name);
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    auto obj3 = object_ref{*pptr_client};

    auto pptr_server = new named_duplex_pipe_server(*pptr_client);

    if (!pptr_server) {
        print("new failed\n");
        free(name);
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    auto obj4 = object_ref{*pptr_server};

    status = name_object(*pptr_client, string_view(name + dev_prefix.size(), namelen - dev_prefix.size()),
                         false);
    if (!success(status)) {
        print("name_object returned {}\n", status);
        free(name);
        return status;
    }

    free(name);

    handle_t h;

    status = t->proc->add_handle(*pptr_server, desired_access, true, h);
    if (!success(status))
        return status;

    *file_handle = h;
    iosb->Information = FILE_CREATED;

    return nt_status::SUCCESS;
}
