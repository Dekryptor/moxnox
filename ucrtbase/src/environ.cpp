import ucrtbase;
import print;

extern "C"
char* getenv(const char* name) {
    print_stderr("FIXME - getenv \"{}\"\n", name);

    // FIXME

    return nullptr;
}
