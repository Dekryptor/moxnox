#include "../../modules/stubs.S"

stubs_start

do_stub "__acrt_iob_func"
do_stub "__setusermatherr"
do_stub "__stdio_common_vfprintf"
do_stub "_cexit"
do_stub "_configthreadlocale"
do_stub "_crt_atexit"
do_stub "_execute_onexit_table"
do_stub "_exit"
do_stub "_initialize_onexit_table"
do_stub "_register_onexit_function"
do_stub "exit"
do_stub "signal"

stubs_end
