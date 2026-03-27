#pragma once
#include <cstdint>
#include <vector>
#include "SendBuffer.h"

constexpr uint16_t MAX_PACKET_SIZE       = 65531; // uint16_t max(65535) - header(4)
constexpr size_t   MAX_RECV_STREAM_SIZE  = 8192;

#pragma pack(push, 1)
struct PacketHeader
{
    uint16_t size;
    uint16_t opcode;
};
#pragma pack(pop)

static_assert(sizeof(PacketHeader) == 4, "PacketHeader must be 4 bytes");

struct Packet
{
    PacketHeader    header{};
    std::vector<char> payload;
};

enum class PacketReadResult : uint8_t
{
    Success,
    Incomplete,
    InvalidHeader,
    PacketTooLarge
};

std::vector<char> MakePacket(uint16_t opcode, const void* payload, uint16_t payloadLen);

inline std::vector<char> MakePacket(uint16_t opcode)
{
    return MakePacket(opcode, nullptr, 0);
}

SendBufferRef MakeSendBuffer(const std::vector<char>& packet);

bool IsValidOpcode(uint16_t opcode);

PacketReadResult TryGetPacket(std::vector<char>& stream, Packet& out);
