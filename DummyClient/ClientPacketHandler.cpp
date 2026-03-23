#include "ClientPacketHandler.h"
#include "Session.h"
#include "Packet.h"
#include "Opcodes.h"

#include <iostream>
#include <string>

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
    std::string msg;
    if (pkt.payloadLen > 0 && pkt.payload)
        msg.assign(pkt.payload, pkt.payload + pkt.payloadLen);

    std::cout << "[Dummy " << _clientId << "] CHAT: " << msg << "\n";
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