#include <array>
#include <wchar.h>

using namespace std;

static const array types = {
    _CONTROL, // 0x00
    _CONTROL,
    _CONTROL,
    _CONTROL,
    _CONTROL,
    _CONTROL,
    _CONTROL,
    _CONTROL,
    _CONTROL,
    _CONTROL | _BLANK | _SPACE,
    _CONTROL | _SPACE,
    _CONTROL | _SPACE,
    _CONTROL | _SPACE,
    _CONTROL | _SPACE,
    _CONTROL,
    _CONTROL,
    _CONTROL, // 0x10
    _CONTROL,
    _CONTROL,
    _CONTROL,
    _CONTROL,
    _CONTROL,
    _CONTROL,
    _CONTROL,
    _CONTROL,
    _CONTROL,
    _CONTROL,
    _CONTROL,
    _CONTROL,
    _CONTROL,
    _CONTROL,
    _CONTROL,
    _BLANK | _SPACE, // 0x20
    _PUNCT,
    _PUNCT,
    _PUNCT,
    _PUNCT,
    _PUNCT,
    _PUNCT,
    _PUNCT,
    _PUNCT,
    _PUNCT,
    _PUNCT,
    _PUNCT,
    _PUNCT,
    _PUNCT,
    _PUNCT,
    _PUNCT,
    _DIGIT | _HEX, // 0x30
    _DIGIT | _HEX,
    _DIGIT | _HEX,
    _DIGIT | _HEX,
    _DIGIT | _HEX,
    _DIGIT | _HEX,
    _DIGIT | _HEX,
    _DIGIT | _HEX,
    _DIGIT | _HEX,
    _DIGIT | _HEX,
    _PUNCT,
    _PUNCT,
    _PUNCT,
    _PUNCT,
    _PUNCT,
    _PUNCT,
    _PUNCT, // 0x40
    _UPPER | _HEX,
    _UPPER | _HEX,
    _UPPER | _HEX,
    _UPPER | _HEX,
    _UPPER | _HEX,
    _UPPER | _HEX,
    _UPPER,
    _UPPER,
    _UPPER,
    _UPPER,
    _UPPER,
    _UPPER,
    _UPPER,
    _UPPER,
    _UPPER,
    _UPPER, // 0x50
    _UPPER,
    _UPPER,
    _UPPER,
    _UPPER,
    _UPPER,
    _UPPER,
    _UPPER,
    _UPPER,
    _UPPER,
    _UPPER,
    _PUNCT,
    _PUNCT,
    _PUNCT,
    _PUNCT,
    _PUNCT,
    _PUNCT, // 0x60
    _LOWER | _HEX,
    _LOWER | _HEX,
    _LOWER | _HEX,
    _LOWER | _HEX,
    _LOWER | _HEX,
    _LOWER | _HEX,
    _LOWER,
    _LOWER,
    _LOWER,
    _LOWER,
    _LOWER,
    _LOWER,
    _LOWER,
    _LOWER,
    _LOWER,
    _LOWER, // 0x70
    _LOWER,
    _LOWER,
    _LOWER,
    _LOWER,
    _LOWER,
    _LOWER,
    _LOWER,
    _LOWER,
    _LOWER,
    _LOWER,
    _PUNCT,
    _PUNCT,
    _PUNCT,
    _PUNCT,
    _CONTROL,
};

extern "C"
int isblank(int c) {
    if (c < 0 || (size_t)c >= types.size())
        return 0;

    return types[c] & _BLANK;
}

extern "C"
int toupper(int c) {
    // FIXME - do this properly

    if (c >= 'a' && c <= 'z')
        return c - 'a' + 'A';
    else
        return c;
}

extern "C"
int isalpha(int c) {
    if (c < 0 || (size_t)c >= types.size())
        return 0;

    return types[c] & (_UPPER | _LOWER);
}

extern "C"
int isupper(int c) {
    if (c < 0 || (size_t)c >= types.size())
        return 0;

    return types[c] & _UPPER;
}

extern "C"
int islower(int c) {
    if (c < 0 || (size_t)c >= types.size())
        return 0;

    return types[c] & _LOWER;
}

extern "C"
int isprint(int c) {
    if (c < 0 || (size_t)c >= types.size())
        return 0;

    return c == ' ' || (types[c] & (_UPPER | _LOWER | _DIGIT | _PUNCT));
}

extern "C"
int iscntrl(int c) {
    if (c < 0 || (size_t)c >= types.size())
        return 0;

    return types[c] & _CONTROL;
}

extern "C"
int ispunct(int c) {
    if (c < 0 || (size_t)c >= types.size())
        return 0;

    return types[c] & _PUNCT;
}

extern "C"
int isgraph(int c) {
    if (c < 0 || (size_t)c >= types.size())
        return 0;

    return types[c] & (_UPPER | _LOWER | _DIGIT | _PUNCT);
}

extern "C"
int isalnum(int c) {
    if (c < 0 || (size_t)c >= types.size())
        return 0;

    return types[c] & (_UPPER | _LOWER | _DIGIT);
}
