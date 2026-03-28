module;

#include <span>
#include <atomic>
#include <mutex>

export module keyboard;

import nt;
import print;
import evdev;
import list;
import thread;
import spinlock;
import handle;
import object_ref;
import moxnox_ioctls;
import chardev;
import event;
import scheduler;
import rwsem;

using namespace std;

static object_ref keyboard_pipe_obj;

class keyboard_pipe : public chardev {
public:
    keyboard_pipe() {
        exclusive = false;
    }

    nt_status read(span<uint8_t>, uint64_t&) override {
        return nt_status::NOT_SUPPORTED;
    }

    nt_status write(span<const uint8_t> s, uint64_t& written_bytes) override;
    nt_status userspace_open(object_ref& objref) override;
    void post_event(const input_event& ev, uint32_t id);

    rwsem post_event_mutex;
    spinlock client_lock;
    list_entry clients = { &clients, &clients };
};

struct input_event_with_id_item {
    list_entry le;
    input_event_with_id ev;
};

struct input_event_item {
    list_entry le;
    input_event ev;
};

class keyboard_pipe_client : public chardev {
public:
    keyboard_pipe_client(keyboard_pipe& p) : p(p) {
        p.acquire();

        {
            lock_guard lock(p.client_lock);
            insert_tail_list(p.clients, le);
        }

        signalled = false;
    }

    ~keyboard_pipe_client() {
        {
            lock_guard lock(p.client_lock);
            remove_entry_list(le);
        }

        while (true) {
            input_event_with_id_item* it = nullptr;

            {
                lock_guard lock(items_lock);

                if (is_list_empty(items))
                    break;

                it = &containing_record<input_event_with_id_item>(items.next);
                remove_entry_list(it->le);
            }

            delete it;
        }

        p.release();
    }

    nt_status read(span<uint8_t> s, uint64_t& read_bytes) override;

    nt_status write(span<const uint8_t> s, uint64_t& written_bytes) override {
        return p.write(s, written_bytes);
    }

    void post_event(const input_event& ev, uint32_t id);

    list_entry le;
    bool visited = false;
    spinlock items_lock;
    list_entry items = { &items, &items };
    size_t items_left = 256;

private:
    keyboard_pipe& p;
};

static atomic<uint32_t> next_keyboard_id = 0;
static spinlock keyboard_contexts_spinlock;
static list_entry keyboard_contexts = { &keyboard_contexts, &keyboard_contexts };

export class evdev_device : public chardev {
public:
    // FIXME - empty ctor to work round GCC bug
    evdev_device() : chardev() { }

    nt_status read(span<uint8_t>, uint64_t&) override {
        return nt_status::NOT_SUPPORTED;
    }

    nt_status write(span<const uint8_t>, uint64_t&) override {
        return nt_status::NOT_SUPPORTED;
    }

    nt_status userspace_open(object_ref& objref) override;

    void post_event(const input_event& ev);

    spinlock client_lock;
    list_entry clients = { &clients, &clients };

private:
    rwsem post_event_mutex;
};

export class evdev_device_client : public chardev {
public:
    evdev_device_client(evdev_device& p) : p(p) {
        p.acquire();

        {
            lock_guard lock(p.client_lock);
            insert_tail_list(p.clients, le);
        }

        signalled = false;
    }

    ~evdev_device_client() {
        {
            lock_guard lock(p.client_lock);
            remove_entry_list(le);
        }

        while (true) {
            input_event_item* it = nullptr;

            {
                lock_guard lock(items_lock);

                if (is_list_empty(items))
                    break;

                it = &containing_record<input_event_item>(items.next);
                remove_entry_list(it->le);
            }

            delete it;
        }

        p.release();
    }

    nt_status read(span<uint8_t> s, uint64_t& read_bytes) override;

    nt_status write(span<const uint8_t> s, uint64_t& written_bytes) override {
        return p.write(s, written_bytes);
    }

    void post_event(const input_event& ev);

    list_entry le;
    bool visited = false;
    spinlock items_lock;
    list_entry items = { &items, &items };
    size_t items_left = 256;

private:
    evdev_device& p;
};

export class keyboard_context {
public:
    keyboard_context(chardev& p) : p(p) {
        p.acquire();

        id = ++next_keyboard_id;

        {
            lock_guard lock(keyboard_contexts_spinlock);
            insert_tail_list(keyboard_contexts, this->le);
        }
    }

    ~keyboard_context() {
        {
            lock_guard lock(keyboard_contexts_spinlock);
            remove_entry_list(this->le);
        }

        p.release();
    }

    void acquire() {
        refcount++;
    }

    void release() {
        if (--refcount == 0)
            delete this;
    }

    nt_status write(const input_event& ev) {
        uint64_t written_bytes;

        return p.write(span((const uint8_t*)&ev, sizeof(input_event)), written_bytes);
    }

    void listen_thread();
    void unregister();

    bool dead = false;
    uint32_t id;
    list_entry le;

private:
    chardev& p;
    atomic<unsigned int> refcount = 1;
};

export bool init_keyboard_pipe() {
    auto pptr = new keyboard_pipe;
    if (!pptr) {
        print("new failed\n");
        return false;
    }

    keyboard_pipe_obj = object_ref{*pptr};

    auto status = name_object(keyboard_pipe_obj.get(), "keyboard");
    if (!success(status)) {
        print("name_object returned {}\n", status);
        return false;
    }

    return true;
}

void keyboard_context::listen_thread() {
    // FIXME - make sure this thread dies if keyboard goes away

    while (true) {
        input_event ev;
        nt_status status;

        auto sp = span((uint8_t*)&ev, sizeof(ev));

        while (true) {
            uint64_t read_bytes;

            status = p.read(sp, read_bytes);
            if (dead || !success(status))
                break;

            if (read_bytes == sp.size())
                break;

            sp = span(sp.data() + read_bytes, sp.size() - read_bytes);
        }

        if (dead)
            break;

        if (!success(status)) {
            print("evdev_device::read returned {}\n", status);
            break;
        }

        if (ev.type != evdev_type::EV_KEY)
            continue;

        auto& k = static_cast<keyboard_pipe&>(keyboard_pipe_obj.get());

        k.post_event(ev, this->id);
    }
}

void keyboard_context::unregister() {
    dead = true;
}

export keyboard_context* register_keyboard(evdev_device& p) {
    object_ref objref{p};

    p.acquire();

    auto status = p.userspace_open(objref);
    if (!success(status)) {
        print("evdev_device::userspace_open returned {}\n", status);
        return nullptr;
    }

    auto kc = new keyboard_context(static_cast<chardev&>(objref.get()));
    if (!kc) {
        print("new failed\n");
        return nullptr;
    }

    add_kernel_thread([](void* ctx) {
        auto& kc = *(keyboard_context*)ctx;

        kc.listen_thread();

        delete &kc;
    }, kc);

    return kc;
}

nt_status keyboard_pipe::userspace_open(object_ref& objref) {
    auto c = new keyboard_pipe_client(*this);
    if (!c) {
        print("new failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    object_ref r{*c};

    objref.swap(r);

    return nt_status::SUCCESS;
}

nt_status keyboard_pipe_client::read(span<uint8_t> s, uint64_t& read_bytes) {
    if (s.size() < sizeof(input_event_with_id))
        return nt_status::BUFFER_TOO_SMALL;

    read_bytes = 0;

    while (true) {
        auto status = wait_for_object(*this, true);
        if (!success(status))
            return status;

        while (true) {
            input_event_with_id_item* it = nullptr;

            {
                lock_guard lock(items_lock);

                if (!is_list_empty(items) && s.size() >= sizeof(input_event_with_id)) {
                    auto& it2 = containing_record<input_event_with_id_item>(items.next);

                    remove_entry_list(it2.le);
                    it = &it2;
                    items_left++;
                }
            }

            if (!it) {
                if (read_bytes > 0)
                    return nt_status::SUCCESS;

                break;
            }

            {
                sync_lock sl;
                lock_guard lock(sl);
                lock_guard lock2(items_lock);

                if (is_list_empty(items))
                    signalled = false;
            }

            memcpy(s.data(), &it->ev, sizeof(input_event_with_id));
            s = s.subspan(sizeof(input_event_with_id));

            delete it;

            read_bytes += sizeof(input_event_with_id);
        }
    }
}

void keyboard_pipe::post_event(const input_event& ev, uint32_t id) {
    lock_guard lg(post_event_mutex);

    {
        lock_guard lock(client_lock);

        if (is_list_empty(clients))
            return;
    }

    while (true) {
        keyboard_pipe_client* c = nullptr;
        bool done = false;

        {
            lock_guard lock(client_lock);

            for (auto le = clients.next; le != &clients; le = le->next) {
                auto& c2 = containing_record<keyboard_pipe_client>(le);

                if (c2.visited) {
                    done = true;
                    break;
                }

                c = &c2;
                c2.acquire();
                c2.visited = true;

                remove_entry_list(c2.le);
                insert_tail_list(clients, c2.le);
            }
        }

        if (done)
            break;

        c->post_event(ev, id);
        c->release();
    }

    {
        lock_guard lock(client_lock);

        for (auto le = clients.next; le != &clients; le = le->next) {
            auto& c = containing_record<keyboard_pipe_client>(le);

            c.visited = false;
        }
    }
}

void keyboard_pipe_client::post_event(const input_event& ev, uint32_t id) {
    // FIXME - drop oldest event if over limit

    auto it = new input_event_with_id_item;
    if (!it) {
        print("new failed\n");
        return;
    }

    it->ev.ev = ev;
    it->ev.id = id;

    bool failed = false;

    {
        lock_guard lock(items_lock);

        if (items_left == 0)
            failed = true;
        else {
            insert_tail_list(items, it->le);
            items_left--;
        }
    }

    if (failed) {
        // FIXME - Linux posts SYN_DROPPED event
        delete it;
        return;
    }

    {
        sync_lock sl;
        lock_guard lock(sl);
        lock_guard lock2(items_lock);

        if (!is_list_empty(items) && !signalled) {
            signalled = true;
            wake_up_waiters(waiters);
        }
    }
}

nt_status keyboard_pipe::write(span<const uint8_t> s, uint64_t& written_bytes) {
    if (s.size() < sizeof(input_event_with_id))
        return nt_status::INVALID_PARAMETER;

    auto& ev = *(input_event_with_id*)s.data();

    keyboard_context* ctx = nullptr;

    {
        lock_guard lock(keyboard_contexts_spinlock);

        for (auto le = keyboard_contexts.next; le != &keyboard_contexts; le = le->next) {
            auto& c = containing_record<keyboard_context>(le);

            if (c.id == ev.id) {
                c.acquire();
                ctx = &c;
                break;
            }
        }
    }

    if (!ctx)
        return nt_status::NOT_FOUND;

    auto status = ctx->write(ev.ev);

    if (!success(status))
        written_bytes = sizeof(input_event_with_id);

    ctx->release();

    return status;
}

nt_status evdev_device_client::read(span<uint8_t> s, uint64_t& read_bytes) {
    if (s.size() < sizeof(input_event))
        return nt_status::BUFFER_TOO_SMALL;

    read_bytes = 0;

    while (true) {
        auto status = wait_for_object(*this, true);
        if (!success(status))
            return status;

        while (true) {
            input_event_item* it = nullptr;

            {
                lock_guard lock(items_lock);

                if (!is_list_empty(items) && s.size() >= sizeof(input_event)) {
                    auto& it2 = containing_record<input_event_item>(items.next);

                    remove_entry_list(it2.le);
                    it = &it2;
                    items_left++;
                }
            }

            if (!it) {
                if (read_bytes > 0)
                    return nt_status::SUCCESS;

                break;
            }

            {
                sync_lock sl;
                lock_guard lock(sl);
                lock_guard lock2(items_lock);

                if (is_list_empty(items))
                    signalled = false;
            }

            memcpy(s.data(), &it->ev, sizeof(input_event));
            s = s.subspan(sizeof(input_event));

            delete it;

            read_bytes += sizeof(input_event);
        }
    }
}

void evdev_device_client::post_event(const input_event& ev) {
    // FIXME - drop oldest event if over limit

    auto it = new input_event_item;
    if (!it) {
        print("new failed\n");
        return;
    }

    it->ev = ev;

    // FIXME - populate time field
    it->ev.time.tv_sec = 0;
    it->ev.time.tv_usec = 0;

    bool failed = false;

    {
        lock_guard lock(items_lock);

        if (items_left == 0)
            failed = true;
        else {
            insert_tail_list(items, it->le);
            items_left--;
        }
    }

    if (failed) {
        // FIXME - Linux posts SYN_DROPPED event
        delete it;
        return;
    }

    {
        sync_lock sl;
        lock_guard lock(sl);
        lock_guard lock2(items_lock);

        if (!is_list_empty(items) && !signalled) {
            signalled = true;
            wake_up_waiters(waiters);
        }
    }
}

nt_status evdev_device::userspace_open(object_ref& objref) {
    auto c = new evdev_device_client(*this);
    if (!c) {
        print("new failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    object_ref r{*c};

    objref.swap(r);

    return nt_status::SUCCESS;
}

void evdev_device::post_event(const input_event& ev) {
    lock_guard lg(post_event_mutex);

    {
        lock_guard lock(client_lock);

        if (is_list_empty(clients))
            return;
    }

    while (true) {
        evdev_device_client* c = nullptr;
        bool done = false;

        {
            lock_guard lock(client_lock);

            for (auto le = clients.next; le != &clients; le = le->next) {
                auto& c2 = containing_record<evdev_device_client>(le);

                if (c2.visited) {
                    done = true;
                    break;
                }

                c = &c2;
                c2.acquire();
                c2.visited = true;

                remove_entry_list(c2.le);
                insert_tail_list(clients, c2.le);
            }
        }

        if (done)
            break;

        c->post_event(ev);
        c->release();
    }

    {
        lock_guard lock(client_lock);

        for (auto le = clients.next; le != &clients; le = le->next) {
            auto& c = containing_record<evdev_device_client>(le);

            c.visited = false;
        }
    }
}
