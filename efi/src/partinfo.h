#pragma once


#define EFI_PARTITION_INFO_PROTOCOL_GUID { 0x8cf2f62c, 0xbc9b, 0x4821, { 0x80, 0x8d, 0xec, 0x9e, 0xc4, 0x21, 0xa1, 0xa0 }};

#define EFI_PARTITION_INFO_PROTOCOL_REVISION 0x0001000
#define PARTITION_TYPE_OTHER 0x00
#define PARTITION_TYPE_MBR 0x01
#define PARTITION_TYPE_GPT 0x02

struct EFI_PARTITION_INFO_PROTOCOL {
    uint32_t Revision;
    uint32_t Type;
    uint8_t System;
    uint8_t Reserved[7];

    union {
        MBR_PARTITION_RECORD Mbr;
        EFI_PARTITION_ENTRY Gpt;
    } Info;
} __attribute__((packed));
