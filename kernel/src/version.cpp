module;

#include <string_view>

export module version;

import loaderblock;

using namespace std;

export decltype(loader_block::version) version_buf;
export string_view version;
