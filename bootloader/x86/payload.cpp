// SPDX-License-Identifier: GPL-2.0-only
extern void* __image_base__;
static void* base = &__image_base__; // TESTING

extern "C"
void start() {
    while (true) {
    }
}
