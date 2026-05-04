extern "C"
int isblank(int c) {
    if (c == '\t' || c == ' ')
        return 1;
    else
        return 0;
}
