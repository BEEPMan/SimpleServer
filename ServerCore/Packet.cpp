#include "Packet.h"
#include "Opcodes.h"
#include <cstring>

std::vector<char> MakePacket(uint16_t opcode, const void* payload, uint16_t payloadLen)
{
    PacketHeader header{};
    header.size = static_cast<uint16_t>(sizeof(PacketHeader) + payloadLen);
    header.opcode = opcode;

    std::vector<char> pkt(header.size);
    std::memcpy(pkt.data(), &header, sizeof(header));

    if (payloadLen > 0 && payload != nullptr)
        std::memcpy(pkt.data() + sizeof(header), payload, payloadLen);

    return pkt;
}

SendBufferRef MakeSendBuffer(const std::vector<char>& packet)
{
    if (packet.empty())
        return nullptr;

    auto sendBuffer = std::make_shared<SendBuffer>(static_cast<int32_t>(packet.size()));
    sendBuffer->CopyFrom(packet.data(), static_cast<int32_t>(packet.size()));
    return sendBuffer;
}

bool IsValidOpcode(uint16_t opcode)
{
    switch (opcode)
    {
    case OP_C_ENTER_GAME:
    case OP_S_ENTER_GAME:
    case OP_S_SPAWN:
    case OP_S_DESPAWN:
    case OP_C_MOVE:
    case OP_S_MOVE:
    case OP_C_CHAT:
    case OP_S_CHAT:
    case OP_S_PLAYER_LIST:
    case OP_C_PING:
    case OP_S_PONG:
    case OP_C_INPUT_CMD:
    case OP_S_PLAYER_STATE:
        return true;
    default:
        return false;
    }
}

PacketReadResult TryGetPacket(std::vector<char>& stream, Packet& out)
{
    if (stream.size() < sizeof(PacketHeader))
        return PacketReadResult::Incomplete;

    PacketHeader header{};
    std::memcpy(&header, stream.data(), sizeof(PacketHeader));

    if (header.size < sizeof(PacketHeader))
        return PacketReadResult::InvalidHeader;

    if (header.size > MAX_PACKET_SIZE)
        return PacketReadResult::PacketTooLarge;

    if (!IsValidOpcode(header.opcode))
        return PacketReadResult::InvalidOpcode;

    if (stream.size() < header.size)
        return PacketReadResult::Incomplete;

    Packet pkt{};
    pkt.header = header;

    const uint16_t payloadLen = static_cast<uint16_t>(header.size - sizeof(PacketHeader));
    if (payloadLen > 0)
        pkt.payload.assign(stream.begin() + sizeof(PacketHeader), stream.begin() + header.size);

    stream.erase(stream.begin(), stream.begin() + header.size);

    out = std::move(pkt);
    return PacketReadResult::Success;
}