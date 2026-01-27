module;

#include <string_view>

export module env;

import nt;

using namespace std;

export u16string_view get_env(u16string_view name) {
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;
    auto env = u16string_view((char16_t*)peb->ProcessParameters->Environment,
                              peb->ProcessParameters->EnvironmentSize / sizeof(char16_t));

    while (true) {
        if (env.empty() || env[0] == 0)
            break;

        auto zero = env.find(u'\0');

        u16string_view bit;

        if (zero == u16string_view::npos)
            bit = env;
        else
            bit = u16string_view(env.data(), zero);

        if (auto eq = bit.find(u'='); eq != u16string_view::npos) {
            if (u16string_view(bit.data(), eq) == name)
                return u16string_view(bit.data() + eq + 1, bit.size() - eq - 1);
        }

        if (zero != u16string_view::npos)
            env = u16string_view(env.data() + zero + 1, env.size() - zero - 1);
        else
            break;
    }

    return u"";
}
