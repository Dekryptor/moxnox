#include "dll_stubs.S"

stubs_start

do_stub "ctermid"
do_stub "ftok"
do_stub "shmat"
do_stub "shmctl"
do_stub "shmdt"
do_stub "shmget"
do_stub "semctl"
do_stub "semget"
do_stub "semop"
do_stub "msgctl"
do_stub "msgget"
do_stub "msgrcv"
do_stub "msgsnd"

stubs_end
