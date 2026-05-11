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
