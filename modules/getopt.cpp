module;

#include <string_view>
#include <span>
#include <tuple>
#include <variant>
#include <stdint.h>
#include <stdlib.h>

export module getopt;

import nt;
import print;

using namespace std;

using opt_func = void(*)();
using opt_param_func = void(*)(u16string_view param);

export struct option {
    u16string_view full_form;
    char16_t short_form;
    variant<opt_func, opt_param_func> func;
};

#ifdef __llvm__ // work around Clang *really* wanting exceptions to be enabled
extern "C++"
exception::~exception() {
}

extern "C++"
const char* exception::what() const noexcept {
    return nullptr;
}

extern "C++"
void operator delete(void* ptr) noexcept {
    free(ptr);
}

extern "C++"
void operator delete(void* ptr, size_t) noexcept {
    free(ptr);
}
#endif

export bool getopt(span<u16string_view>& argv, span<const option> options) {
    while (!argv.empty()) {
        auto arg = argv.front();

        if (arg.empty()) {
            argv = span(argv.data() + 1, argv.size() - 1);
            continue;
        }

        if (arg[0] != '-')
            break;

        if (arg.size() >= 2 && arg[1] == '-') { // full form
            auto full = u16string_view(arg.data() + 2, arg.size() - 2);

            if (full.empty()) { // --
                argv = span(argv.data() + 1, argv.size() - 1);
                break;
            }

            bool done = false;
            for (auto o : options) {
                if (full == o.full_form) {
                    if (holds_alternative<opt_func>(o.func))
                        get<opt_func>(o.func)();
                    else if (holds_alternative<opt_param_func>(o.func)) {
                        if (argv.size() == 1) {
                            print_stderr("option requires an argument -- '--{}'\n", full);
                            return false;
                        }

                        get<opt_param_func>(o.func)(argv[1]);
                        argv = span(argv.data() + 1, argv.size() - 1);
                    }

                    done = true;
                    break;
                }
            }

            if (!done) {
                print_stderr("unrecognized option '--{}'\n", full); // FIXME - prefix with name of program
                return false;
            }
        } else { // short form
            arg = u16string_view(arg.data() + 1, arg.size() - 1); // skip -

            while (!arg.empty()) {
                bool done = false;
                for (auto o : options) {
                    if (arg[0] == o.short_form) {
                        if (holds_alternative<opt_func>(o.func))
                            get<opt_func>(o.func)();
                        else if (holds_alternative<opt_param_func>(o.func)) {
                            auto& func = get<opt_param_func>(o.func);

                            if (arg.size() > 1) {
                                func(u16string_view(arg.data() + 1, arg.size() - 1));
                                arg = u"-";
                            } else if (argv.size() > 1) {
                                func(argv[1]);
                                argv = span(argv.data() + 1, argv.size() - 1);
                            } else {
                                print_stderr("option requires an argument -- '-{}'\n", u16string_view(arg.data(), 1));
                                return false;
                            }
                        }

                        done = true;
                        break;
                    }
                }

                if (!done) {
                    print_stderr("invalid option -- '{}'\n", u16string_view(arg.data(), 1)); // FIXME - prefix with name of program
                    return false;
                }

                arg = u16string_view(arg.data() + 1, arg.size() - 1);
            }
        }

        argv = span(argv.data() + 1, argv.size() - 1);
    }

    return true;
}
