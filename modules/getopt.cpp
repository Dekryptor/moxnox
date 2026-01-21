module;

#include <string_view>
#include <span>
#include <tuple>
#include <stdint.h>

export module getopt;

import nt;
import format;

using namespace std;

export struct option {
    u16string_view full_form;
    char16_t short_form;
    void(*func)();
};

template<typename... Args>
void print(type_identity_t<checker<type_identity_t<Args>...>> fmt, Args... args) {
    char buf[4096];
    auto teb = NtCurrentTeb();
    auto peb = teb->ProcessEnvironmentBlock;
    IO_STATUS_BLOCK iosb;

    auto sp = span(buf, sizeof(buf));

    auto t = make_tuple(args...);

    sp = format_to(sp, fmt.sv, t);

    auto sv = string_view(buf, (size_t)(sp.data() - buf));

    NtWriteFile(peb->ProcessParameters->StandardError, 0, nullptr, nullptr,
                &iosb, sv.data(), (uint32_t)sv.size(), nullptr, nullptr);
}

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
                    o.func();
                    done = true;
                    break;
                }
            }

            if (!done) {
                print("unrecognized option '--{}'\n", full); // FIXME - prefix with name of program
                return false;
            }
        } else { // short form
            arg = u16string_view(arg.data() + 1, arg.size() - 1); // skip -

            while (!arg.empty()) {
                bool done = false;
                for (auto o : options) {
                    if (arg[0] == o.short_form) {
                        o.func();
                        done = true;
                        break;
                    }
                }

                if (!done) {
                    print("invalid option -- '{}'\n", u16string_view(arg.data(), 1)); // FIXME - prefix with name of program
                    return false;
                }

                arg = u16string_view(arg.data() + 1, arg.size() - 1);
            }
        }

        argv = span(argv.data() + 1, argv.size() - 1);
    }

    return true;
}
