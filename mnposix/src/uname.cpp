// SPDX-License-Identifier: LGPL-2.1-or-later
#include <sys/utsname.h>
#include <string.h>
#include <string_view>
#include <algorithm>

import timepage;

using namespace std;

static string_view get_os_version() {
    const auto& time_page = *(time_page_struct*)TIME_PAGE_BASE;

    auto sv = string_view(time_page.os_version);

    if (auto sp = sv.find(' '); sp != string_view::npos)
        sv = string_view(sv.data() + sp + 1, sv.size() - sp - 1);

    return sv;
}

static string_view get_os_hash() {
    const auto& time_page = *(time_page_struct*)TIME_PAGE_BASE;

    return string_view(time_page.os_hash);
}

extern "C"
int uname(struct utsname* buf) {
    char hostname[sizeof(time_page_struct::hostname)];

    auto version = get_os_version();
    auto hash = get_os_hash();

    strcpy(buf->sysname, "Moxnox");

    read_host_name(hostname);

    auto len = strlen(hostname);

    memcpy(buf->nodename, hostname, min(len, sizeof(buf->nodename) - 1));

    if (len < sizeof(buf->nodename))
        buf->nodename[len] = 0;
    else
        buf->nodename[sizeof(buf->nodename) - 1] = 0;

    memcpy(buf->release, version.data(), version.size());
    buf->release[version.size()] = 0;

    memcpy(buf->version, hash.data(), hash.size());
    buf->version[hash.size()] = 0;

#ifdef __x86_64__
    strcpy(buf->machine, "x86_64");
#elifdef __aarch64__
    strcpy(buf->machine, "aarch64");
#endif

    return 0;
}
