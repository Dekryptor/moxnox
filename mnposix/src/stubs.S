#include "dll_stubs.S"

stubs_start

do_stub "getlocalename_l"
do_stub "mlock"
do_stub "munlock"
do_stub "alphasort"
do_stub "pthread_atfork"
do_stub "pthread_cancel"
do_stub "crypt"
do_stub "ctermid"
do_stub "setlogmask"
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
