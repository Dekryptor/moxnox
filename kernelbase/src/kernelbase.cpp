// SPDX-License-Identifier: LGPL-2.1-or-later
#include <stdint.h>

import nt;

extern "C"
int DllMainCRTStartup(HANDLE /*hinstDLL*/, uint32_t /*dwReason*/,
                      void* /*lpvReserved*/) {
    return 1;
}
