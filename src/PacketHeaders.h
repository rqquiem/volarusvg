// src/PacketHeaders.h
#ifndef PACKETHEADERS_H
#define PACKETHEADERS_H

#include <stdint.h>

#pragma pack(push, 1)

// ============================================================================
// Layer 2: Ethernet
// ============================================================================
struct EthernetHeader {
    uint8_t dest[6];
    uint8_t src[6];
    uint16_t type;      // 0x0800=IPv4, 0x0806=ARP, 0x86DD=IPv6
};

// ============================================================================
// Layer 2.5: ARP
// ============================================================================
struct ArpHeader {
    uint16_t hw_type;
    uint16_t proto_type;
    uint8_t hw_size;
    uint8_t proto_size;
    uint16_t opcode;    // 1=Request, 2=Reply
    uint8_t sender_mac[6];
    uint32_t sender_ip;
    uint8_t target_mac[6];
    uint32_t target_ip;
};

struct FullArpPacket {
    EthernetHeader eth;
    ArpHeader arp;
};

// ============================================================================
// Layer 3: IPv4
// ============================================================================
struct IPv4Header {
    uint8_t  ver_ihl;       // version (4 bits) + IHL (4 bits)
    uint8_t  tos;           // type of service / DSCP
    uint16_t total_length;  // total packet length
    uint16_t id;            // identification
    uint16_t flags_frag;    // flags (3 bits) + fragment offset (13 bits)
    uint8_t  ttl;           // time to live
    uint8_t  protocol;      // 6=TCP, 17=UDP, 1=ICMP
    uint16_t checksum;      // header checksum
    uint32_t src_addr;      // source IP
    uint32_t dst_addr;      // destination IP

    uint8_t headerLength() const { return (ver_ihl & 0x0F) * 4; }
    uint8_t version() const { return (ver_ihl >> 4) & 0x0F; }
};

// ============================================================================
// Layer 4: TCP
// ============================================================================
struct TcpHeader {
    uint16_t src_port;
    uint16_t dst_port;
    uint32_t seq_num;
    uint32_t ack_num;
    uint8_t  data_offset;   // upper 4 bits = offset in 32-bit words
    uint8_t  flags;         // FIN=0x01, SYN=0x02, RST=0x04, PSH=0x08, ACK=0x10, URG=0x20
    uint16_t window;
    uint16_t checksum;
    uint16_t urgent_ptr;

    uint8_t headerLength() const { return ((data_offset >> 4) & 0x0F) * 4; }
    bool isSYN() const { return flags & 0x02; }
    bool isACK() const { return flags & 0x10; }
    bool isFIN() const { return flags & 0x01; }
    bool isRST() const { return flags & 0x04; }
    bool isPSH() const { return flags & 0x08; }
};

// ============================================================================
// Layer 4: UDP
// ============================================================================
struct UdpHeader {
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t length;
    uint16_t checksum;
};

// ============================================================================
// Layer 4: ICMP
// ============================================================================
struct IcmpHeader {
    uint8_t  type;      // 0=Echo Reply, 8=Echo Request, 3=Dest Unreachable, etc.
    uint8_t  code;
    uint16_t checksum;
    uint32_t rest;      // type-specific (id+seq for echo, gateway for redirect, etc.)
};

#pragma pack(pop)

// ============================================================================
// EtherType Constants
// ============================================================================
#define ETHERTYPE_IPV4  0x0800
#define ETHERTYPE_ARP   0x0806
#define ETHERTYPE_IPV6  0x86DD

// IP Protocol Constants
#define IPPROTO_ICMP_   1
#define IPPROTO_TCP_    6
#define IPPROTO_UDP_    17

#endif // PACKETHEADERS_H
