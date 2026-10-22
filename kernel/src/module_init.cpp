// SPDX-License-Identifier: GPL-2.0-only
export module module_init;

export class module_init {
public:
    module_init(void (*func)()) {
        func();
    }
};
