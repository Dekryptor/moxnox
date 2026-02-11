import ucrtbase;
import print;

extern "C" __declspec(dllexport)
int puts(const char* s) {
    print("FIXME - puts\n");

    return 0;
}
