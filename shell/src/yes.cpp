#include <ntdll.h>
#include "shell.h"

using namespace std;

extern "C"
void startup() {
    while (true) {
        print("y\n");
    }
}
