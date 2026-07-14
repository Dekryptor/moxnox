#pragma once

#undef _alloca
#define _alloca(x) __builtin_alloca((x))
#undef alloca
#define alloca(x) __builtin_alloca((x))
