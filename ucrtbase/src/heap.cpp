#include <utility>

import ucrtbase;

using namespace std;

static int new_mode;

extern "C" __declspec(dllexport)
int _set_new_mode(int mode) {
    swap(mode, new_mode);

    return mode;
}
