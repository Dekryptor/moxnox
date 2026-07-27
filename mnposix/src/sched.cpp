import nt;
import ucrtbase;

#undef errno
import errno;

extern "C"
int sched_yield() {
    auto status = NtYieldExecution();
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    return 0;
}
