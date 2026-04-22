add_library(harfbuzz SHARED
    repo/src/hb-aat-layout.cc
    repo/src/hb-blob.cc
    repo/src/hb-buffer.cc
    repo/src/hb-common.cc
    repo/src/hb-face-builder.cc
    repo/src/hb-face.cc
    repo/src/hb-fallback-shape.cc
    repo/src/hb-font.cc
    repo/src/hb-ft.cc
    repo/src/hb-number.cc
    repo/src/hb-ot-face.cc
    repo/src/hb-ot-font.cc
    repo/src/hb-ot-layout.cc
    repo/src/hb-ot-map.cc
    repo/src/hb-ot-metrics.cc
    repo/src/hb-ot-shape.cc
    repo/src/hb-ot-shape-fallback.cc
    repo/src/hb-ot-shape-normalize.cc
    repo/src/hb-ot-shaper-arabic.cc
    repo/src/hb-ot-shaper-default.cc
    repo/src/hb-ot-shaper-hangul.cc
    repo/src/hb-ot-shaper-hebrew.cc
    repo/src/hb-ot-shaper-indic.cc
    repo/src/hb-ot-shaper-indic-table.cc
    repo/src/hb-ot-shaper-khmer.cc
    repo/src/hb-ot-shaper-myanmar.cc
    repo/src/hb-ot-shaper-syllabic.cc
    repo/src/hb-ot-shaper-thai.cc
    repo/src/hb-ot-shaper-use.cc
    repo/src/hb-ot-shaper-vowel-constraints.cc
    repo/src/hb-ot-tag.cc
    repo/src/hb-set.cc
    repo/src/hb-shape.cc
    repo/src/hb-shape-plan.cc
    repo/src/hb-shaper.cc
    repo/src/hb-static.cc
    repo/src/hb-ucd.cc
    repo/src/hb-unicode.cc
)
target_sources(harfbuzz PUBLIC FILE_SET CXX_MODULES FILES harfbuzz.cpp)

target_compile_definitions(harfbuzz PRIVATE -DHAVE_FREETYPE -DHB_LEAN -DHB_NO_AAT -DHB_NO_MT)
target_compile_options(harfbuzz PRIVATE -mno-stack-arg-probe -Wa,-mbig-obj -fno-exceptions -fno-rtti ${USERSPACE_CFLAGS} -w)
target_compile_options(harfbuzz PRIVATE -include ${CMAKE_CURRENT_SOURCE_DIR}/hbshim.h)
target_include_directories(harfbuzz PRIVATE repo/src)
target_link_libraries(harfbuzz PRIVATE freetype)
target_link_libraries(harfbuzz PRIVATE ${LIBGCC})

install(TARGETS harfbuzz DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
    ARCHIVE EXCLUDE_FROM_ALL
    CXX_MODULES_BMI EXCLUDE_FROM_ALL
)
