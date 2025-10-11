#pragma once

import page_cache;

enum class section_type {
    anonymous,
    file,
    image
};

class anon_section_cache {
public:
    size_t read_into_cache(uint64_t page_num, uint64_t num_pages);

    page_cache<anon_section_cache> cache{*this};
};
