#include "ClientPacketHandler.h"
#include "Session.h"
#include "Packet.h"
#include "Opcodes.h"

#include <iostream>
#include <string_view>

ClientPacketHandler::ClientPacketHandler(int clientId)
    : _clientId(clientId)
{
}

bool ClientPacketHandler::HandlePacket(Session& session, const Packet& pkt)
{
    switch (pkt.header.opcode)
    {
    case OP_CHAT: return HandleChat(session, pkt);
    case OP_PING: return HandlePing(session, pkt);
    case OP_PONG: return HandlePong(session, pkt);
    default:           return false;
    }
}

bool ClientPacketHandler::HandleChat(Session&, const Packet& pkt)
{
    const std::string_view msg(pkt.payload.data(), pkt.payload.size());

    std::cout << "[Dummy " << _clientId << "] chat: " << msg << "\n";
    return true;
}

bool ClientPacketHandler::HandlePing(Session& session, const Packet&)
{
    std::cout << "[Dummy " << _clientId << "] PING received\n";
    session.EnqueueSend(MakePacket(OP_PONG));
    return true;
}

bool ClientPacketHandler::HandlePong(Session&, const Packet&)
{
    std::cout << "[Dummy " << _clientId << "] PONG received\n";
    return true;
}