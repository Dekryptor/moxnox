import print;

extern "C"
int setenv(const char* name, const char* value, int overwrite) {
    print("FIXME - setenv ({}, {}, {})\n", name, value, overwrite);

    return 0;
}
