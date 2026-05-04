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
