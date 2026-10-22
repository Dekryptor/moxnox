// SPDX-License-Identifier: GPL-2.0-only
module;

#include <string_view>

export module version;

import loaderblock;

using namespace std;

export decltype(loader_block::version) version_buf;
export string_view version;
export decltype(loader_block::hash) os_hash_buf;
export string_view os_hash;
