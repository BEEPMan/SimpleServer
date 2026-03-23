#include "ServerPacketHandler.h"
#include "Session.h"
#include "Packet.h"
#include "NetService.h"
#include "Opcodes.h"

#include <iostream>
#include <string>

bool ServerPacketHandler::HandlePacket(Session& session, const Packet& pkt)
{
    switch (pkt.header.opcode)
    {
    case OP_CHAT: return HandleChat(session, pkt);
    case OP_PING: return HandlePing(session, pkt);
    case OP_PONG: return HandlePong(session, pkt);
    default:           return false;
    }
}

bool ServerPacketHandler::HandleChat(Session&, const Packet& pkt)
{
    if (_service == nullptr)
        return false;

    std::string msg;
    if (pkt.payloadLen > 0 && pkt.payload)
        msg.assign(pkt.payload, pkt.payload + pkt.payloadLen);

    std::cout << "[Server] Chat received: " << msg << "\n";

    auto out = MakePacket(OP_CHAT, msg.data(), static_cast<uint16_t>(msg.size()));
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