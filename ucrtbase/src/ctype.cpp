extern "C"
int isblank(int c) {
    if (c == '\t' || c == ' ')
        return 1;
    else
        return 0;
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
    if (c >= 'A' && c <= 'Z')
        return 1;
    else if (c >= 'a' && c <= 'z')
        return 1;
    else
        return 0;
}

extern "C"
int isupper(int c) {
    if (c >= 'A' && c <= 'Z')
        return 1;
    else
        return 0;
}

extern "C"
int islower(int c) {
    if (c >= 'a' && c <= 'z')
        return 1;
    else
        return 0;
}

extern "C"
int isprint(int c) {
    if (c >= 'A' && c <= 'Z')
        return 1;
    else if (c >= 'a' && c <= 'z')
        return 1;
    else if (c >= '0' && c <= '9')
        return 1;
    else if (c >= '!' && c <= '/')
        return 1;
    else if (c >= ':' && c <= '@')
        return 1;
    else if (c >= '[' && c <= '`')
        return 1;
    else if (c >= '{' && c <= '~')
        return 1;
    else if (c == ' ')
        return 1;
    else
        return 0;
}

extern "C"
int iscntrl(int c) {
    if (c >= 0 && c <= 0x1f)
        return 1;
    else if (c == 0x7f)
        return 1;
    else
        return 0;
}

extern "C"
int ispunct(int c) {
    if (c >= '!' && c <= '/')
        return 1;
    else if (c >= ':' && c <= '@')
        return 1;
    else if (c >= '[' && c <= '`')
        return 1;
    else if (c >= '{' && c <= '~')
        return 1;
    else
        return 0;
}

extern "C"
int isgraph(int c) {
    if (c >= 'A' && c <= 'Z')
        return 1;
    else if (c >= 'a' && c <= 'z')
        return 1;
    else if (c >= '0' && c <= '9')
        return 1;
    else if (c >= '!' && c <= '/')
        return 1;
    else if (c >= ':' && c <= '@')
        return 1;
    else if (c >= '[' && c <= '`')
        return 1;
    else if (c >= '{' && c <= '~')
        return 1;
    else
        return 0;
}

extern "C"
int isalnum(int c) {
    if (c >= 'A' && c <= 'Z')
        return 1;
    else if (c >= 'a' && c <= 'z')
        return 1;
    else if (c >= '0' && c <= '9')
        return 1;
    else
        return 0;
}
