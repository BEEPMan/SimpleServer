#pragma once
#include "IPacketHandler.h"

class ClientPacketHandler : public IPacketHandler
{
public:
    explicit ClientPacketHandler(int clientId);

    bool HandlePacket(Session& session, const Packet& pkt) override;

private:
    bool HandleChat(Session&, const Packet& pkt);
    bool HandlePing(Session& session, const Packet&);
    bool HandlePong(Session&, const Packet&);

private:
    int _clientId = 0;
};