import ucrtbase;
import print;

using jmp_buf = void*; // FIXME!

extern "C"
int _setjmp(jmp_buf /*env*/) {
    print("FIXME - _setjmp\n");

    return 0;
}
