#include <stdint.h>
#include <stdio.h>
#include <bit>

import print;
import ipv6;
import ws2_32;

using namespace std;

//=============================================================================
// Test 1: Resolve hostname to IPv6
//=============================================================================
static void test_resolve_hostname() {
    print("\nTest 1: Resolve hostname (google.com)\n");
    print("======================================\n");

    struct addrinfo hints = {};
    hints.ai_family = AF_INET6;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo* result;
    int ret = getaddrinfo("google.com", "80", &hints, &result);

    if (ret != 0) {
        print("  FAIL: getaddrinfo returned error: {}\n", gai_strerror(ret));
        return;
    }

    print("  ✓ DNS resolution succeeded\n");

    // Print all results
    int count = 0;
    for (struct addrinfo* rp = result; rp != NULL; rp = rp->ai_next) {
        count++;

        struct sockaddr_in6* addr6 = (struct sockaddr_in6*)rp->ai_addr;

        print("  Result {}: [{}]:{}\n", count, *(ipv6_address*)&addr6->sin6_addr, byteswap(addr6->sin6_port));
        print("    Family: {}\n", rp->ai_family == AF_INET6 ? "AF_INET6" : "?");
        print("    Type: {}\n", rp->ai_socktype == SOCK_STREAM ? "SOCK_STREAM" : "SOCK_DGRAM");
        print("    Protocol: {}\n", rp->ai_protocol);
    }

    if (count == 0)
        print("  FAIL: No results returned\n");
    else
        print("  ✓ Got {} result(s)\n", count);

    freeaddrinfo(result);
}

//=============================================================================
// Test 2: Parse numeric IPv6 address
//=============================================================================
static void test_numeric_ipv6() {
    print("\nTest 2: Numeric IPv6 address\n");
    print("=============================\n");

    struct addrinfo hints = {};
    hints.ai_family = AF_INET6;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_NUMERICHOST;  // Don't do DNS lookup

    struct addrinfo* result;
    int ret = getaddrinfo("2001:4860:4860::8888", "53", &hints, &result);

    if (ret != 0) {
        print("  FAIL: getaddrinfo returned error: {}\n", gai_strerror(ret));
        return;
    }

    print("  ✓ Numeric parse succeeded\n");

    struct sockaddr_in6* addr6 = (struct sockaddr_in6*)result->ai_addr;

    print("  Address: [{}]:{}\n", *(ipv6_address*)&addr6->sin6_addr, byteswap(addr6->sin6_port));

    if (*(ipv6_address*)&addr6->sin6_addr == parse_ipv6_address("2001:4860:4860::8888"))
        print("  ✓ Address parsed correctly\n");
    else
        print("  FAIL: Address not parsed correctly\n");

    freeaddrinfo(result);
}

//=============================================================================
// Test 3: Service name to port
//=============================================================================
static void test_service_name() {
    print("\nTest 3: Service name to port\n");
    print("=============================\n");

    const char* services[] = {"http", "https", "ssh", "dns", "ftp"};
    uint16_t expected_ports[] = {80, 443, 22, 53, 21};

    for (unsigned int i = 0; i < 5; i++) {
        struct addrinfo hints = {};
        hints.ai_family = AF_INET6;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_NUMERICHOST;

        struct addrinfo* result;
        int ret = getaddrinfo("::1", services[i], &hints, &result);

        if (ret != 0) {
            print("  {} → FAIL: {}\n", services[i], gai_strerror(ret));
            continue;
        }

        struct sockaddr_in6* addr6 = (struct sockaddr_in6*)result->ai_addr;

        if (byteswap(addr6->sin6_port) == expected_ports[i])
            print("  {} → {} ✓\n", services[i], byteswap(addr6->sin6_port));
        else
            print("  {} → {} (expected {}) ✗\n", services[i], byteswap(addr6->sin6_port), expected_ports[i]);

        freeaddrinfo(result);
    }
}

//=============================================================================
// Test 4: AI_PASSIVE (server mode)
//=============================================================================
static void test_ai_passive() {
    print("\nTest 4: AI_PASSIVE (server mode)\n");
    print("=================================\n");

    struct addrinfo hints = {};
    hints.ai_family = AF_INET6;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    struct addrinfo* result;
    int ret = getaddrinfo(NULL, "8080", &hints, &result);

    if (ret != 0) {
        print("  FAIL: getaddrinfo returned error: {}\n", gai_strerror(ret));
        return;
    }

    struct sockaddr_in6* addr6 = (struct sockaddr_in6*)result->ai_addr;

    if (*(ipv6_address*)&addr6->sin6_addr == parse_ipv6_address("::") && byteswap(addr6->sin6_port) == 8080)
        print("  ✓ AI_PASSIVE returned [::]:8080\n");
    else {
        print("  Got [{}]:{}\n", *(ipv6_address*)&addr6->sin6_addr, byteswap(addr6->sin6_port));

        if (*(ipv6_address*)&addr6->sin6_addr != parse_ipv6_address("::"))
            print("  FAIL: Expected :: (any address)\n");

        if (byteswap(addr6->sin6_port) != 8080)
            print("  FAIL: Expected port 8080\n");
    }

    freeaddrinfo(result);
}

//=============================================================================
// Test 5: IPv6 with scope ID
//=============================================================================
static void test_scope_id() {
    print("\nTest 5: IPv6 with scope ID\n");
    print("===========================\n");

    struct addrinfo hints = {};
    hints.ai_family = AF_INET6;
    hints.ai_flags = AI_NUMERICHOST;

    // Test with numeric scope
    struct addrinfo* result;
    int ret = getaddrinfo("fe80::1%2", "80", &hints, &result);

    if (ret != 0) {
        print("  FAIL: getaddrinfo returned error: {}\n", gai_strerror(ret));
        return;
    }

    struct sockaddr_in6* addr6 = (struct sockaddr_in6*)result->ai_addr;

    print("  Address: [{}]%{}\n", *(ipv6_address*)&addr6->sin6_addr, addr6->sin6_scope_id);

    if (addr6->sin6_scope_id == 2)
        print("  ✓ Scope ID parsed correctly\n");
    else
        print("  FAIL: Expected scope ID 2, got {}\n", addr6->sin6_scope_id);

    freeaddrinfo(result);
}

//=============================================================================
// Test 6: Multiple results
//=============================================================================
static void test_multiple_results() {
    print("\nTest 6: Multiple results\n");
    print("========================\n");

    struct addrinfo hints = {};
    hints.ai_family = AF_UNSPEC;  // Both IPv4 and IPv6
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo* result;
    int ret = getaddrinfo("google.com", "443", &hints, &result);

    if (ret != 0) {
        print("  FAIL: getaddrinfo returned error: {}\n", gai_strerror(ret));
        return;
    }

    int ipv4_count = 0, ipv6_count = 0;

    for (struct addrinfo* rp = result; rp != NULL; rp = rp->ai_next) {
        if (rp->ai_family == AF_INET6)
            ipv6_count++;
        else if (rp->ai_family == AF_INET)
            ipv4_count++;
    }

    print("  IPv6 results: {}\n", ipv6_count);
    print("  IPv4 results: {}\n", ipv4_count);

    if (ipv6_count > 0 || ipv4_count > 0)
        print("  ✓ Got multiple results\n");
    else
        print("  FAIL: No results\n");

    freeaddrinfo(result);
}

//=============================================================================
// Test 7: Error cases
//=============================================================================
static void test_error_cases() {
    print("\nTest 7: Error cases\n");
    print("===================\n");

    struct addrinfo hints = {};
    hints.ai_family = AF_INET6;
    struct addrinfo* result;

    // Test 1: Invalid hostname
    print("  Invalid hostname: ");
    int ret = getaddrinfo("this-domain-does-not-exist-12345.com", "80", &hints, &result);

    if (ret != 0)
        print("✓ Error: {}\n", gai_strerror(ret));
    else {
        print("✗ Should have failed\n");
        freeaddrinfo(result);
    }

    // Test 2: Numeric host with AI_NUMERICHOST but invalid format
    print("  Invalid numeric: ");
    hints.ai_flags = AI_NUMERICHOST;
    ret = getaddrinfo("not-an-ip-address", "80", &hints, &result);

    if (ret != 0)
        print("✓ Error: {}\n", gai_strerror(ret));
    else {
        print("✗ Should have failed\n");
        freeaddrinfo(result);
    }

    // Test 3: Invalid service
    print("  Invalid service: ");
    hints.ai_flags = 0;
    ret = getaddrinfo("::1", "not-a-service", &hints, &result);

    if (ret != 0)
        print("✓ Error: {}\n", gai_strerror(ret));
    else {
        print("✗ Should have failed\n");
        freeaddrinfo(result);
    }
}

//=============================================================================
// Test 8: Actual connection attempt
//=============================================================================
static void test_connection() {
    print("\nTest 8: Actual connection\n");
    print("=========================\n");

    struct addrinfo hints = {};
    hints.ai_family = AF_INET6;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo* result;
    int ret = getaddrinfo("google.com", "80", &hints, &result);

    if (ret != 0) {
        print("  FAIL: getaddrinfo: {}\n", gai_strerror(ret));
        return;
    }

    print("  Attempting to connect to Google...\n");

    int sock = -1;
    for (struct addrinfo* rp = result; rp != NULL; rp = rp->ai_next) {
        sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sock < 0) {
            print("  Socket creation failed\n");
            continue;
        }

        struct sockaddr_in6* addr6 = (struct sockaddr_in6*)rp->ai_addr;

        print("  Trying [{}]:80...\n", *(ipv6_address*)&addr6->sin6_addr);

        if (connect(sock, rp->ai_addr, (int)rp->ai_addrlen) == 0) {
            print("  ✓ Connected successfully!\n");
            break;
        }

        print("  Connection failed, trying next...\n");
        closesocket(sock);
        sock = -1;
    }

    if (sock >= 0) {
        print("  ✓ Test passed - connection works\n");
        closesocket(sock);
    } else
        print("  FAIL: Could not connect to any address\n");

    freeaddrinfo(result);
}

//=============================================================================
// Test 9: Edge cases
//=============================================================================
static void test_edge_cases() {
    print("\nTest 9: Edge cases\n");
    print("==================\n");

    struct addrinfo hints = {};
    struct addrinfo* result;

    // Test 1: NULL service
    print("  NULL service: ");
    hints.ai_family = AF_INET6;
    hints.ai_flags = AI_NUMERICHOST;
    int ret = getaddrinfo("::1", nullptr, &hints, &result);

    if (ret == 0) {
        struct sockaddr_in6* addr6 = (struct sockaddr_in6*)result->ai_addr;

        print("✓ Port = {}\n", byteswap(addr6->sin6_port));
        freeaddrinfo(result);
    } else
        print("✗ Error: {}\n", gai_strerror(ret));

    // Test 2: Port 0
    print("  Port 0: ");
    ret = getaddrinfo("::1", "0", &hints, &result);
    if (ret == 0) {
        struct sockaddr_in6* addr6 = (struct sockaddr_in6*)result->ai_addr;

        if (byteswap(addr6->sin6_port) == 0)
            print("✓ Port = 0\n");
        else
            print("✗ Expected 0, got {}\n", byteswap(addr6->sin6_port));

        freeaddrinfo(result);
    } else
        print("✗ Error: {}\n", gai_strerror(ret));

    // Test 3: Loopback (::1)
    print("  Loopback: ");
    ret = getaddrinfo("::1", "80", &hints, &result);

    if (ret == 0) {
        struct sockaddr_in6* addr6 = (struct sockaddr_in6*)result->ai_addr;

        if (*(ipv6_address*)&addr6->sin6_addr == parse_ipv6_address("::1"))
            print("✓ ::1\n");
        else
            print("✗ Not ::1\n");

        freeaddrinfo(result);
    } else
        print("✗ Error: {}\n", gai_strerror(ret));
}

//=============================================================================
// Main
//=============================================================================
int main() {
    print("getaddrinfo() Test Suite\n");
    print("========================\n");

    test_resolve_hostname();
    test_numeric_ipv6();
    test_service_name();
    test_ai_passive();
    test_scope_id();
    test_multiple_results();
    test_error_cases();
    test_connection();
    test_edge_cases();

    print("\n========================\n");
    print("Tests complete!\n");

    return 0;
}
