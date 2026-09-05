set(BUILD_SHARED_LIBS ON)
set(HB_HAVE_FREETYPE ON)
set(HB_BUILD_SUBSET OFF)
set(HB_BUILD_UTILS OFF)
set(SKIP_INSTALL_ALL ON)

set(CMAKE_MESSAGE_LOG_LEVEL WARNING)
add_subdirectory(repo)

target_sources(harfbuzz PUBLIC FILE_SET CXX_MODULES FILES harfbuzz.cpp)

set_target_properties(harfbuzz PROPERTIES LINKER_LANGUAGE CXX)
target_compile_features(harfbuzz PUBLIC cxx_std_${CMAKE_CXX_STANDARD})

target_compile_definitions(harfbuzz PRIVATE HB_LEAN HB_NO_AAT HB_NO_MT)

get_target_property(libs harfbuzz LINK_LIBRARIES)
list(REMOVE_ITEM libs Threads::Threads)
set_target_properties(harfbuzz PROPERTIES LINK_LIBRARIES "${libs}")

get_target_property(libs harfbuzz INTERFACE_LINK_LIBRARIES)
list(REMOVE_ITEM libs Threads::Threads)
set_target_properties(harfbuzz PROPERTIES INTERFACE_LINK_LIBRARIES "${libs}")

target_compile_options(harfbuzz PRIVATE
    ${USERSPACE_CFLAGS}
    -mno-stack-arg-probe
    -Wno-float-conversion
    -Wno-attributes
    -include ${CMAKE_CURRENT_SOURCE_DIR}/hbshim.h
    $<$<CXX_COMPILER_ID:GNU>:-Wa,-mbig-obj>
)

install(TARGETS harfbuzz DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
    ARCHIVE EXCLUDE_FROM_ALL
    CXX_MODULES_BMI EXCLUDE_FROM_ALL
)
