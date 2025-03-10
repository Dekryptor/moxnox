#include <ntdll.h>
#include <string_view>

import shelllib;

using namespace std;

extern "C"
void startup() {
    while (true) {
        print("y\n");
    }
}
