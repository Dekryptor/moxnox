#include "dll_stubs.S"

stubs_start

do_stub_no_export "tcflush"
do_stub_no_export "nanosleep64"
do_stub_no_export "nanosleep"
do_stub_no_export "sleep"
do_stub_no_export "gettimeofday"
do_stub_no_export "realpath"
do_stub_no_export "dlopen"
do_stub_no_export "dlsym"
do_stub_no_export "dlclose"
do_stub_no_export "dlerror"
do_stub_no_export "posix_spawn_file_actions_init"
do_stub_no_export "posix_spawn_file_actions_adddup2"
do_stub_no_export "posix_spawn_file_actions_addclose"
do_stub_no_export "posix_spawnp"
do_stub_no_export "posix_spawn"
do_stub_no_export "posix_spawn_file_actions_destroy"
do_stub_no_export "posix_spawnattr_destroy"
do_stub_no_export "posix_spawnattr_init"
do_stub_no_export "fcntl"
do_stub_no_export "kill"

stubs_end
