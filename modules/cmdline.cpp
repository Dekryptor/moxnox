module;

#include <string_view>
#include <span>

using namespace std;

export module cmdline;

export unsigned int count_cmdline_args(u16string_view cmdline) {
    unsigned int argc = 0;

    while (!cmdline.empty()) {
        while (cmdline.front() == ' ' || cmdline.front() == '\t') {
            cmdline = u16string_view(cmdline.data() + 1, cmdline.size() - 1);
        }

        if (cmdline.empty())
            break;

        size_t end = cmdline.size();

        if (cmdline.front() == u'"') {
            if (auto q = cmdline.find(u'"', 1); q != string_view::npos && q < end) {
                end = q - 1;
                cmdline = u16string_view(cmdline.data() + end + 2, cmdline.size() - end - 2);
            } else
                cmdline = u"";
        } else {
            if (auto sp = cmdline.find(u' '); sp != string_view::npos && sp < end)
                end = sp;

            if (auto tab = cmdline.find(u'\t'); tab != string_view::npos && tab < end)
                end = tab;

            cmdline = u16string_view(cmdline.data() + end, cmdline.size() - end);
        }

        argc++;
    }

    return argc;
}

export void parse_cmdline(u16string_view cmdline, span<u16string_view> argv) {
    // FIXME - escaping quotes with backslashes

    while (!cmdline.empty() && !argv.empty()) {
        auto& arg = argv.front();

        while (cmdline.front() == ' ' || cmdline.front() == '\t') {
            cmdline = u16string_view(cmdline.data() + 1, cmdline.size() - 1);
        }

        if (cmdline.empty())
            break;

        size_t end = cmdline.size();

        if (cmdline.front() == u'"') {
            if (auto q = cmdline.find(u'"', 1); q != string_view::npos && q < end) {
                end = q - 1;
                arg = u16string_view(cmdline.data() + 1, end);
                cmdline = u16string_view(cmdline.data() + end + 2, cmdline.size() - end - 2);
            } else {
                arg = u16string_view(cmdline.data() + 1, end - 1);
                cmdline = u"";
            }
        } else {
            if (auto sp = cmdline.find(u' '); sp != string_view::npos && sp < end)
                end = sp;

            if (auto tab = cmdline.find(u'\t'); tab != string_view::npos && tab < end)
                end = tab;

            arg = u16string_view(cmdline.data(), end);
            cmdline = u16string_view(cmdline.data() + end, cmdline.size() - end);
        }

        argv = argv.subspan(1);
    }
}
