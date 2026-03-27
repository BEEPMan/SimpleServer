#include "ProtoUtil.h"

std::vector<char> MakePacketFromProto(uint16_t opcode, const google::protobuf::MessageLite& msg)
{
    std::string serialized;

    if (!msg.SerializeToString(&serialized))
        return {};

    if (serialized.size() > MAX_PACKET_SIZE - sizeof(PacketHeader))
        return {};

    if (serialized.empty())
        return {};

    return MakePacket(opcode, serialized.data(), static_cast<uint16_t>(serialized.size()));
}