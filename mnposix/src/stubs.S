#include "dll_stubs.S"

stubs_start

do_stub "ctermid"
do_stub "msgctl"
do_stub "msgget"
do_stub "msgrcv"
do_stub "msgsnd"

stubs_end
