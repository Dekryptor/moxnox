import ws2_32;
import print;

extern "C"
int getaddrinfo(const char* node, const char* service, const addrinfo* hints,
                addrinfo** res) {
    print_stderr("FIXME - getaddrinfo\n");

    return EAI_FAIL;
}

extern "C"
void freeaddrinfo(addrinfo* res) {
    print_stderr("FIXME - freeaddrinfo\n");
}

extern "C"
const char* gai_strerror(int errcode) {
    print_stderr("FIXME - gai_strerror {}", errcode);

    return "?";
}
