import ucrtbase;
import print;

extern "C" __declspec(dllexport)
int _set_new_mode(int new_mode) {
    print("FIXME - _set_new_mode\n");

    return 0;
}
