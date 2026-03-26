#include "ServerPacketHandler.h"
#include "Session.h"
#include "Packet.h"
#include "NetService.h"
#include "Opcodes.h"

#include <iostream>
#include <string_view>

bool ServerPacketHandler::HandlePacket(Session& session, const Packet& pkt)
{
    switch (pkt.header.opcode)
    {
    case OP_CHAT: return HandleChat(session, pkt);
    case OP_PING: return HandlePing(session, pkt);
    case OP_PONG: return HandlePong(session, pkt);
    default:      return false;
    }
}

bool ServerPacketHandler::HandleChat(Session&, const Packet& pkt)
{
    if (_service == nullptr)
        return false;

    const std::string_view msg(pkt.payload.data(), pkt.payload.size());

    std::cout << "[서버] 채팅 수신: " << msg << "\n";

    auto out = MakePacket(OP_CHAT, pkt.payload.data(), static_cast<uint16_t>(pkt.payload.size()));
    _service->Broadcast(std::move(out));
    return true;
}

bool ServerPacketHandler::HandlePing(Session& session, const Packet&)
{
    session.EnqueueSend(MakePacket(OP_PONG));
    return true;
}

bool ServerPacketHandler::HandlePong(Session&, const Packet&)
{
    return true;
}