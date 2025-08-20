module;

#include <string_view>

export module cmdline;

using namespace std;

export string_view cmdline;

template<typename T>
concept cmdline_func = invocable<T, string_view> &&
requires(T t, string_view s) { { t(s) } -> convertible_to<bool>; };

export void loop_through_cmdline(cmdline_func auto func) {
    string_view s = cmdline;

    while (true) {
        if (auto c = s.find_first_not_of(' '); c != string_view::npos) {
            s.remove_prefix(c);
        }

        if (s.empty())
            break;

        if (auto c = s.find_first_of(' '); c != string_view::npos) {
            if (!func(string_view(s.data(), c)))
                break;

            s.remove_prefix(c);
        } else {
            func(s);
            break;
        }
    }
}
