#include <netinet/in.h>

extern "C"
const struct in6_addr in6addr_any = { { 0, 0, 0, 0, 0, 0, 0, 0,
                                        0, 0, 0, 0, 0, 0, 0, 0 } };

extern "C"
const struct in6_addr in6addr_loopback = { { 0, 0, 0, 0, 0, 0, 0, 0,
                                             0, 0, 0, 0, 0, 0, 0, 1 } };
