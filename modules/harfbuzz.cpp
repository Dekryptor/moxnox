module;

#include <hb.h>
#include <hb-ft.h>

export module harfbuzz;

export using ::hb_font_t;
export using ::hb_font_t;
export using ::hb_glyph_position_t;
export using ::hb_buffer_t;
export using ::hb_codepoint_t;
export using ::hb_glyph_info_t;

export using ::hb_buffer_create;
export using ::hb_buffer_destroy;
export using ::hb_buffer_guess_segment_properties;
export using ::hb_buffer_get_length;
export using ::hb_buffer_get_glyph_infos;
export using ::hb_buffer_add_codepoints;
export using ::hb_buffer_get_glyph_positions;
export using ::hb_ft_font_create;
export using ::hb_shape;
