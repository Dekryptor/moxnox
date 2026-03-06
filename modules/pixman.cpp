module;

#include <pixman.h>

export module pixman;

extern "C" {

export using ::pixman_bool_t;

export using ::pixman_fixed_32_32_t;
export using ::pixman_fixed_48_16_t;
export using ::pixman_fixed_1_31_t;
export using ::pixman_fixed_1_16_t;
export using ::pixman_fixed_16_16_t;
export using ::pixman_fixed_t;

// FIXME - pixman_fixed_e
// FIXME - pixman_fixed_1
// FIXME - pixman_fixed_1_minus_e
// FIXME - pixman_fixed_minus_1
// FIXME - pixman_fixed_to_int(f)
// FIXME - pixman_int_to_fixed(i)
// FIXME - pixman_fixed_to_double(f)
// FIXME - pixman_double_to_fixed(d)
// FIXME - pixman_fixed_frac(f)
// FIXME - pixman_fixed_floor(f)
// FIXME - pixman_fixed_ceil(f)
// FIXME - pixman_fixed_fraction(f)
// FIXME - pixman_fixed_mod_2(f)
// FIXME - pixman_max_fixed_48_16
// FIXME - pixman_min_fixed_48_16

export using ::pixman_color_t;
export using ::pixman_point_fixed_t;
export using ::pixman_line_fixed_t;
export using ::pixman_vector_t;
export using ::pixman_transform_t;
export using ::pixman_image_t;

export using ::pixman_transform_init_identity;
export using ::pixman_transform_point_3d;
export using ::pixman_transform_point;
export using ::pixman_transform_multiply;
export using ::pixman_transform_init_scale;
export using ::pixman_transform_scale;
export using ::pixman_transform_init_rotate;
export using ::pixman_transform_rotate;
export using ::pixman_transform_init_translate;
export using ::pixman_transform_translate;
export using ::pixman_transform_bounds;
export using ::pixman_transform_invert;
export using ::pixman_transform_is_identity;
export using ::pixman_transform_is_scale;
export using ::pixman_transform_is_int_translate;
export using ::pixman_transform_is_inverse;

export using ::pixman_f_transform_t;
export using ::pixman_f_vector_t;

export using ::pixman_transform_from_pixman_f_transform;
export using ::pixman_f_transform_from_pixman_transform;
export using ::pixman_f_transform_invert;
export using ::pixman_f_transform_point;
export using ::pixman_f_transform_point_3d;
export using ::pixman_f_transform_multiply;
export using ::pixman_f_transform_init_scale;
export using ::pixman_f_transform_scale;
export using ::pixman_f_transform_init_rotate;
export using ::pixman_f_transform_rotate;
export using ::pixman_f_transform_init_translate;
export using ::pixman_f_transform_translate;
export using ::pixman_f_transform_bounds;
export using ::pixman_f_transform_init_identity;

export using enum ::pixman_repeat_t;
export using enum ::pixman_dither_t;
export using enum ::pixman_filter_t;
export using enum ::pixman_op_t;

export using ::pixman_region16_data_t;
export using ::pixman_box16_t;
export using ::pixman_rectangle16_t;
export using ::pixman_region16_t;

export using enum ::pixman_region_overlap_t;

export using ::pixman_region_set_static_pointers;
export using ::pixman_region_init;
export using ::pixman_region_init_rect;
export using ::pixman_region_init_rects;
export using ::pixman_region_init_with_extents;
export using ::pixman_region_init_from_image;
export using ::pixman_region_fini;
export using ::pixman_region_translate;
export using ::pixman_region_copy;
export using ::pixman_region_intersect;
export using ::pixman_region_union;
export using ::pixman_region_union_rect;
export using ::pixman_region_intersect_rect;
export using ::pixman_region_subtract;
export using ::pixman_region_inverse;
export using ::pixman_region_contains_point;
export using ::pixman_region_contains_rectangle;
export using ::pixman_region_empty;
export using ::pixman_region_not_empty;
export using ::pixman_region_extents;
export using ::pixman_region_n_rects;
export using ::pixman_region_rectangles;
export using ::pixman_region_equal;
export using ::pixman_region_selfcheck;
export using ::pixman_region_reset;
export using ::pixman_region_clear;

export using ::pixman_region32_data_t;
export using ::pixman_box32_t;
export using ::pixman_rectangle32_t;
export using ::pixman_region32_t;

export using ::pixman_region32_init;
export using ::pixman_region32_init_rect;
export using ::pixman_region32_init_rects;
export using ::pixman_region32_init_with_extents;
export using ::pixman_region32_init_from_image;
export using ::pixman_region32_fini;
export using ::pixman_region32_translate;
export using ::pixman_region32_copy;
export using ::pixman_region32_intersect;
export using ::pixman_region32_union;
export using ::pixman_region32_intersect_rect;
export using ::pixman_region32_union_rect;
export using ::pixman_region32_subtract;
export using ::pixman_region32_inverse;
export using ::pixman_region32_contains_point;
export using ::pixman_region32_contains_rectangle;
export using ::pixman_region32_empty;
export using ::pixman_region32_not_empty;
export using ::pixman_region32_extents;
export using ::pixman_region32_n_rects;
export using ::pixman_region32_rectangles;
export using ::pixman_region32_equal;
export using ::pixman_region32_selfcheck;
export using ::pixman_region32_reset;
export using ::pixman_region32_clear;

export using ::pixman_region64f_data_t;
export using ::pixman_box64f_t;
export using ::pixman_rectangle64f_t;
export using ::pixman_region64f_t;

export using ::pixman_region64f_init;
export using ::pixman_region64f_init_rect;
export using ::pixman_region64f_init_rectf;
export using ::pixman_region64f_init_rects;
export using ::pixman_region64f_init_with_extents;
export using ::pixman_region64f_init_from_image;
export using ::pixman_region64f_fini;
export using ::pixman_region64f_translate;
export using ::pixman_region64f_translatef;
export using ::pixman_region64f_copy;
export using ::pixman_region64f_intersect;
export using ::pixman_region64f_union;
export using ::pixman_region64f_intersect_rect;
export using ::pixman_region64f_intersect_rectf;
export using ::pixman_region64f_union_rect;
export using ::pixman_region64f_union_rectf;
export using ::pixman_region64f_subtract;
export using ::pixman_region64f_inverse;
export using ::pixman_region64f_contains_point;
export using ::pixman_region64f_contains_pointf;
export using ::pixman_region64f_contains_rectangle;
export using ::pixman_region64f_empty;
export using ::pixman_region64f_not_empty;
export using ::pixman_region64f_extents;
export using ::pixman_region64f_n_rects;
export using ::pixman_region64f_rectangles;
export using ::pixman_region64f_equal;
export using ::pixman_region64f_selfcheck;
export using ::pixman_region64f_reset;
export using ::pixman_region64f_clear;
export using ::pixman_blt;
export using ::pixman_fill;
export using ::pixman_version;
export using ::pixman_version_string;

export using ::pixman_indexed_t;
export using ::pixman_gradient_stop_t;

export using ::pixman_read_memory_func_t;
export using ::pixman_write_memory_func_t;
export using ::pixman_image_destroy_func_t;

// FIXME - PIXMAN_FORMAT(bpp,type,a,r,g,b)
// FIXME - PIXMAN_FORMAT_BYTE(bpp,type,a,r,g,b)
// FIXME - PIXMAN_FORMAT_RESHIFT(val, ofs, num)
// FIXME - PIXMAN_FORMAT_BPP(f)
// FIXME - PIXMAN_FORMAT_SHIFT(f)
// FIXME - PIXMAN_FORMAT_TYPE(f)
// FIXME - PIXMAN_FORMAT_A(f)
// FIXME - PIXMAN_FORMAT_R(f)
// FIXME - PIXMAN_FORMAT_G(f)
// FIXME - PIXMAN_FORMAT_B(f)
// FIXME - PIXMAN_FORMAT_RGB(f)
// FIXME - PIXMAN_FORMAT_VIS(f)
// FIXME - PIXMAN_FORMAT_DEPTH(f)
// FIXME - PIXMAN_TYPE_OTHER
// FIXME - PIXMAN_TYPE_A
// FIXME - PIXMAN_TYPE_ARGB
// FIXME - PIXMAN_TYPE_ABGR
// FIXME - PIXMAN_TYPE_COLOR
// FIXME - PIXMAN_TYPE_GRAY
// FIXME - PIXMAN_TYPE_YUY2
// FIXME - PIXMAN_TYPE_YV12
// FIXME - PIXMAN_TYPE_BGRA
// FIXME - PIXMAN_TYPE_RGBA
// FIXME - PIXMAN_TYPE_ARGB_SRGB
// FIXME - PIXMAN_TYPE_RGBA_FLOAT
//
// FIXME - PIXMAN_FORMAT_COLOR(f)

export using enum ::pixman_format_code_t;

export using ::pixman_format_supported_destination;
export using ::pixman_format_supported_source;
export using ::pixman_image_create_solid_fill;
export using ::pixman_image_create_linear_gradient;
export using ::pixman_image_create_radial_gradient;
export using ::pixman_image_create_conical_gradient;
export using ::pixman_image_create_bits;
export using ::pixman_image_create_bits_no_clear;
export using ::pixman_image_ref;
export using ::pixman_image_unref;
export using ::pixman_image_set_destroy_function;
export using ::pixman_image_get_destroy_data;
export using ::pixman_image_set_clip_region;
export using ::pixman_image_set_clip_region32;
export using ::pixman_image_set_clip_region64f;
export using ::pixman_image_set_has_client_clip;
export using ::pixman_image_set_transform;
export using ::pixman_image_set_repeat;
export using ::pixman_image_set_dither;
export using ::pixman_image_set_dither_offset;
export using ::pixman_image_set_filter;
export using ::pixman_image_set_source_clipping;
export using ::pixman_image_set_alpha_map;
export using ::pixman_image_set_component_alpha;
export using ::pixman_image_get_component_alpha;
export using ::pixman_image_set_accessors;
export using ::pixman_image_set_indexed;
export using ::pixman_image_get_data;
export using ::pixman_image_get_width;
export using ::pixman_image_get_height;
export using ::pixman_image_get_stride;
export using ::pixman_image_get_depth;
export using ::pixman_image_get_format;

export using enum ::pixman_kernel_t;

export using ::pixman_filter_create_separable_convolution;
export using ::pixman_image_fill_rectangles;
export using ::pixman_image_fill_boxes;
export using ::pixman_compute_composite_region;
export using ::pixman_image_composite;
export using ::pixman_image_composite32;
export using ::pixman_image_composite64f;
export using ::pixman_disable_out_of_bounds_workaround;

export using ::pixman_glyph_cache_t;

export using ::pixman_glyph_cache_create;
export using ::pixman_glyph_cache_destroy;
export using ::pixman_glyph_cache_freeze;
export using ::pixman_glyph_cache_thaw;
export using ::pixman_glyph_cache_lookup;
export using ::pixman_glyph_cache_insert;
export using ::pixman_glyph_cache_remove;
export using ::pixman_glyph_get_extents;
export using ::pixman_glyph_get_mask_format;
export using ::pixman_composite_glyphs;
export using ::pixman_composite_glyphs_no_mask;

export using ::pixman_edge_t;
export using ::pixman_trapezoid_t;
export using ::pixman_trap_t;
export using ::pixman_span_fix_t;
export using ::pixman_triangle_t;

// FIXME - pixman_trapezoid_valid

export using ::pixman_sample_ceil_y;
export using ::pixman_sample_floor_y;
export using ::pixman_edge_step;
export using ::pixman_edge_init;
export using ::pixman_line_fixed_edge_init;
export using ::pixman_rasterize_edges;
export using ::pixman_add_traps;
export using ::pixman_add_trapezoids;
export using ::pixman_rasterize_trapezoid;
export using ::pixman_composite_trapezoids;
export using ::pixman_composite_triangles;
export using ::pixman_add_triangles;

}
