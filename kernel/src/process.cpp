#include "kernel.h"
#include "objects.h"
#include "unique_handle.h"

using namespace std;

// FIXME - make sure process signalled when it dies

unique_handle create_process(string_view fn) {
    auto file = open_file(fn);
    if (!file) {
        print("open_file failed\n");
        return {};
    }

    // FIXME - create section from file
    // FIXME - make sure section refers to PE file

    auto pptr = new process;
    if (!pptr) {
        print("new failed\n");
        return {};
    }

    auto& p = *pptr;

    p.img = create_section(file.get(), true);
    if (!p.img) {
        print("create_section failed\n");
        p.release();
        return {};
    }

    // FIXME - map section into process
    // FIXME - map ntdll into process
    // FIXME - map time page into process

    // FIXME - create user thread for process, with stack
    // FIXME - start point of thread should actually be in ntdll, so it can do relocations etc.

    // FIXME - token for process

    // FIXME - add process to list

    auto hv = add_handle(p, 0); // FIXME - access mask

    p.release(); // add_handle increases refcount

    return unique_handle{hv};
}
