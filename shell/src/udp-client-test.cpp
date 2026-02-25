#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <bit>

#undef errno
import print;
import ws2_32;
import ipv6;
import errno;

using namespace std;

static const auto GOOGLE_DNS = parse_ipv6_address("2001:4860:4860::8888");

// Test 1: Basic UDP send/receive with sendto/recvfrom
void test_sendto_recvfrom() {
    print("Test 1: sendto/recvfrom\n");

    // Create UDP socket
    int sock = socket(AF_INET6, SOCK_DGRAM, 0);
    if (sock < 0) {
        print("  FAIL: socket() returned {} (errno {})\n", sock, errno_to_string_view(*_errno()));
        return;
    }
    print("  ✓ Created socket: fd={}\n", sock);

    // Prepare destination (example: DNS query to Google DNS)
    struct sockaddr_in6 dest = {};
    dest.sin6_family = AF_INET6;
    dest.sin6_port = byteswap((uint16_t)53);  // DNS port
    *(ipv6_address*)&dest.sin6_addr = GOOGLE_DNS;

    // Simple DNS query (just a test packet, not valid DNS)
    char query[] = {
        0x12, 0x34,  // Transaction ID
        0x01, 0x00,  // Flags (standard query)
        0x00, 0x01,  // Questions: 1
        0x00, 0x00,  // Answers: 0
        0x00, 0x00,  // Authority: 0
        0x00, 0x00,  // Additional: 0
        // (Would normally have question here, but this is just a test)
    };

    // Send UDP packet
    ssize_t sent = sendto(sock, query, sizeof(query), 0,
                         (struct sockaddr*)&dest, sizeof(dest));

    if (sent < 0) {
        print("  FAIL: sendto() returned {} (errno {})\n", sent, errno_to_string_view(*_errno()));
        closesocket(sock);
        return;
    }
    print("  ✓ Sent {} bytes\n", sent);

    // Receive response (with timeout would be better)
    char response[512];
    struct sockaddr_in6 from = {};
    int fromlen = sizeof(from);

    ssize_t received = recvfrom(sock, response, sizeof(response), 0,
                                (struct sockaddr*)&from, &fromlen);

    if (received < 0)
        print("  FAIL: recvfrom() returned {} (errno {})\n", received, errno_to_string_view(*_errno()));
    else {
        print("  ✓ Received {} bytes from [{}]:{}\n",
              received, *(ipv6_address*)&from.sin6_addr, byteswap(from.sin6_port));

        // Print first few bytes
        print("    Data: ");
        for (ssize_t i = 0; i < (received < 16 ? received : 16); i++) {
            print("{:02x} ", response[i]);
        }
        print("\n");
    }

    closesocket(sock);
    print("  ✓ Closed socket\n");
}

// Test 2: Connected UDP socket (using send/recv)
void test_connected_udp() {
    print("\nTest 2: Connected UDP socket\n");

    int sock = socket(AF_INET6, SOCK_DGRAM, 0);
    if (sock < 0) {
        print("  FAIL: socket() returned {} (errno {})\n", sock, errno_to_string_view(*_errno()));
        return;
    }

    // Connect to destination
    struct sockaddr_in6 dest = {};
    dest.sin6_family = AF_INET6;
    dest.sin6_port = byteswap((uint16_t)53);
    *(ipv6_address*)&dest.sin6_addr = GOOGLE_DNS;

    int ret = connect(sock, (struct sockaddr*)&dest, sizeof(dest));
    if (ret < 0) {
        print("  FAIL: connect() returned {} (errno {})\n", ret, errno_to_string_view(*_errno()));
        closesocket(sock);
        return;
    }
    print("  ✓ Connected socket\n");

    // Now can use send() instead of sendto()
    char query[] = {0x56, 0x78, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00};

    ssize_t sent = send(sock, query, sizeof(query), 0);
    if (sent < 0) {
        print("  FAIL: send() returned {} (errno {})\n", sent, errno_to_string_view(*_errno()));
        closesocket(sock);
        return;
    }
    print("  ✓ Wrote {} bytes using send()\n", sent);

    // Receive using recv()
    char response[512];
    ssize_t received = recv(sock, response, sizeof(response), 0);

    if (received < 0)
        print("  FAIL: recv() returned {} (errno {})\n", received, errno_to_string_view(*_errno()));
    else
        print("  ✓ Read {} bytes using recv()\n", received);

    closesocket(sock);
}

// Test 3: Bind to specific port
void test_bind() {
    print("\nTest 3: Bind to specific port\n");

    int sock = socket(AF_INET6, SOCK_DGRAM, 0);
    if (sock < 0) {
        print("  FAIL: socket() returned {} (errno {})\n", sock, errno_to_string_view(*_errno()));
        return;
    }

    // Bind to specific port
    struct sockaddr_in6 addr = {};
    addr.sin6_family = AF_INET6;
    addr.sin6_port = byteswap((uint16_t)12345);  // Arbitrary port
    // addr.sin6_addr = IN6ADDR_ANY (all zeros = any address)

    int ret = bind(sock, (struct sockaddr*)&addr, sizeof(addr));
    if (ret < 0) {
        print("  FAIL: bind() returned {} (errno {})\n", ret, errno_to_string_view(*_errno()));
        closesocket(sock);
        return;
    }
    print("  ✓ Bound to port 12345\n");

    // Could now receive on this port
    // (Would need another program to send to us)

    closesocket(sock);
}

// Test 4: Multiple sends/receives
void test_multiple_packets() {
    print("\nTest 4: Multiple packets\n");

    int sock = socket(AF_INET6, SOCK_DGRAM, 0);
    if (sock < 0) {
        print("  FAIL: socket() returned {} (errno {})\n", sock, errno_to_string_view(*_errno()));
        return;
    }

    struct sockaddr_in6 dest = {};
    dest.sin6_family = AF_INET6;
    dest.sin6_port = byteswap((uint16_t)53);
    *(ipv6_address*)&dest.sin6_addr = GOOGLE_DNS;

    connect(sock, (struct sockaddr*)&dest, sizeof(dest));

    // Send multiple packets
    for (unsigned int i = 0; i < 5; i++) {
        char packet[16];

        for (unsigned int j = 0; j < 16; j++) {
            packet[j] = (char)((i * 16 + j) & 0xff);
        }

        ssize_t sent = send(sock, packet, sizeof(packet), 0);

        if (sent < 0)
            print("  FAIL: send() returned {} (errno {})\n", sent, errno_to_string_view(*_errno()));
        else
            print("  Packet {}: sent {} bytes\n", i, sent);
    }

    print("  ✓ Sent 5 packets\n");

    closesocket(sock);
}

// Test 5: Large packet (test fragmentation/MTU)
void test_large_packet() {
    print("\nTest 5: Large packet\n");

    int sock = socket(AF_INET6, SOCK_DGRAM, 0);
    if (sock < 0) {
        print("  FAIL: socket() returned {} (errno {})\n", sock, errno_to_string_view(*_errno()));
        return;
    }

    struct sockaddr_in6 dest = {};
    dest.sin6_family = AF_INET6;
    dest.sin6_port = byteswap((uint16_t)53);
    *(ipv6_address*)&dest.sin6_addr = GOOGLE_DNS;

    // Try sending a large packet (close to MTU)
    char large_packet[1400];  // Close to typical MTU
    for (size_t i = 0; i < sizeof(large_packet); i++) {
        large_packet[i] = (char)(i & 0xff);
    }

    ssize_t sent = sendto(sock, large_packet, sizeof(large_packet), 0,
                         (struct sockaddr*)&dest, sizeof(dest));

    if (sent < 0)
        print("  FAIL: sendto() returned {} (errno {})\n", sent, errno_to_string_view(*_errno()));
    else
        print("  ✓ Sent {} bytes (large packet)\n", sent);

    closesocket(sock);
}

// Test 6: Zero-length packet
void test_zero_length() {
    print("\nTest 6: Zero-length packet\n");

    int sock = socket(AF_INET6, SOCK_DGRAM, 0);
    if (sock < 0) {
        print("  FAIL: socket() returned {} (errno {})\n", sock, errno_to_string_view(*_errno()));
        return;
    }

    struct sockaddr_in6 dest = {};
    dest.sin6_family = AF_INET6;
    dest.sin6_port = byteswap((uint16_t)53);
    *(ipv6_address*)&dest.sin6_addr = GOOGLE_DNS;

    // Send zero-length packet (valid in UDP!)
    ssize_t sent = sendto(sock, nullptr, 0, 0, (struct sockaddr*)&dest,
                          sizeof(dest));

    if (sent < 0)
        print("  FAIL: sendto() returned {} (errno {})\n", sent, errno_to_string_view(*_errno()));
    else
        print("  ✓ Sent zero-length packet\n");

    closesocket(sock);
}

int main() {
    print("UDP Client Test Suite\n");
    print("====================\n\n");

    // Run all tests
    test_sendto_recvfrom();
    test_connected_udp();
    test_bind();
    test_multiple_packets();
    test_large_packet();
    test_zero_length();

    print("\n====================\n");
    print("Tests complete!\n");

    return 0;
}
