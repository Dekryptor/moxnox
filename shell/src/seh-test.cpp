import nt;
import ntdll;
import format;
import print;

enum EXCEPTION_DISPOSITION handler2(EXCEPTION_RECORD* rec, void* /*frame*/,
                                    CONTEXT* context,
                                    DISPATCHER_CONTEXT* /*dispatch*/) {
#ifdef __aarch64__
    print("rec->ExceptionCode = {:x}, context->Pc = {:x}\n",
          rec->ExceptionCode, context->Pc);
#elifdef __x86_64__
    print("rec->ExceptionCode = {:x}, context->Rip = {:x}\n",
          rec->ExceptionCode, context->Rip);
#endif

    if (rec->ExceptionCode == 0x12345678)
        return EXCEPTION_DISPOSITION::ExceptionContinueExecution;
    else
        return EXCEPTION_DISPOSITION::ExceptionContinueSearch;
}

extern "C"
enum EXCEPTION_DISPOSITION handler(EXCEPTION_RECORD* rec, void* frame,
                                   CONTEXT* context, DISPATCHER_CONTEXT* dispatch) {
    return handler2(rec, frame, context, dispatch);
}

static void __attribute__((noinline)) seh_test() {
    __asm __volatile(".seh_handler handler, @except");

    EXCEPTION_RECORD er;

    er.ExceptionFlags = 0;
    er.ExceptionRecord = nullptr;
    // er.ExceptionAddress = (void*)ctx.Rip;
    er.ExceptionCode = 0x12345678;
    er.NumberParameters = 0;

    print("before exception\n");

    RtlRaiseException(&er);

    print("after exception\n");

    // *(char*)1 = 1;
}

#ifdef __x86_64__
#define FORCE_ALIGN_ARG_POINTER __attribute__((force_align_arg_pointer))
#else
#define FORCE_ALIGN_ARG_POINTER
#endif

extern "C" FORCE_ALIGN_ARG_POINTER void startup() {
    print("beginning of startup\n");

    seh_test();

    print("end of startup\n");

    NtTerminateThread(NtCurrentThread(), nt_status::SUCCESS);
}
