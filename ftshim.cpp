#include <ntdll.h>
#include <ft2build.h>
#include <freetype/freetype.h>
#include <freetype/ftbitmap.h>

static void* ft_alloc(FT_Memory, long size) {
    return malloc((size_t)size);
}

static void* ft_realloc(FT_Memory, long cur_size, long new_size, void* block) {
    void* ret;

    ret = malloc((size_t)new_size);
    if (!ret)
        return nullptr;

    memcpy(ret, block, cur_size < new_size ? (size_t)new_size : (size_t)cur_size);

    free(block);

    return ret;
}

static void ft_free(FT_Memory, void* block) {
    free(block);
}

extern "C"
FT_Memory FT_New_Memory() {
    auto ptr = (FT_MemoryRec_*)malloc(sizeof(FT_MemoryRec_));
    if (!ptr)
        return nullptr;

    auto& memory = *ptr;

    memory.user = nullptr;
    memory.alloc = ft_alloc;
    memory.realloc = ft_realloc;
    memory.free = ft_free;

    return &memory;
}

extern "C"
void FT_Done_Memory(FT_Memory memory) {
    free(memory);
}

extern "C"
int DllMainCRTStartup(HANDLE hinstDLL, uint32_t dwReason, void* lpvReserved) {
    return 1;
}
