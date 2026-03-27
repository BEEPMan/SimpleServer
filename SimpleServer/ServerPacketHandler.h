#pragma once
#include "IPacketHandler.h"

class NetService;

class ServerPacketHandler : public IPacketHandler
{
public:
    void SetService(NetService* service) { _service = service; }
    bool HandlePacket(Session& session, const Packet& pkt) override;

private:
    bool HandleEnterGame(Session& session, const Packet& pkt);
    bool HandleMove(Session& session, const Packet& pkt);
    bool HandleChat(Session& session, const Packet& pkt);
    bool HandlePing(Session& session, const Packet& pkt);

private:
    NetService* _service = nullptr;
};
