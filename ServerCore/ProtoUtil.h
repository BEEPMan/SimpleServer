#pragma once
#include "Packet.h"
#include "game.pb.h"

// Protobuf 메시지를 직렬화해 패킷 바이트열로 만든다
std::vector<char> MakePacketFromProto(uint16_t opcode, const google::protobuf::MessageLite& msg);
