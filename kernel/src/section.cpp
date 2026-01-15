module;

#include <optional>
#include <functional>
#include <stdint.h>
#include "section.h"

export module section;

import nt;
import file;
import object_ref;

using namespace std;

export nt_status create_section(const optional<reference_wrapper<file>>& file, bool image,
                                size_t size, uint32_t page_protection, object_ref& ref);
export uint64_t get_image_size(void* image);
