import nt;
import print;

extern "C"
void mn_handle_apc(CONTEXT* ctx, PS_APC_ROUTINE apc_routine, void* apc_arg1,
                   void* apc_arg2, void* apc_arg3) {
    apc_routine(apc_arg1, apc_arg2, apc_arg3);
    NtContinue(ctx, true);
}

extern "C"
void mn_posix_signal(int sig) {
    print("FIXME - mn_posix_signal {}\n", sig);
}
