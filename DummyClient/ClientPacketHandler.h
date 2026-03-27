#pragma once
#include "IPacketHandler.h"

class ClientPacketHandler : public IPacketHandler
{
public:
    explicit ClientPacketHandler(int clientId);
    bool HandlePacket(Session& session, const Packet& pkt) override;

private:
    bool HandleEnterGame(Session& session, const Packet& pkt);
    bool HandleSpawn(Session& session, const Packet& pkt);
    bool HandleDespawn(Session& session, const Packet& pkt);
    bool HandleMove(Session& session, const Packet& pkt);
    bool HandleChat(Session& session, const Packet& pkt);
    bool HandlePlayerList(Session& session, const Packet& pkt);
    bool HandlePong(Session& session, const Packet& pkt);

private:
    int _clientId = 0;
};
