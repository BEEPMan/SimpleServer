#pragma once
#include "IPacketHandler.h"

class NetService;

class ServerPacketHandler : public IPacketHandler
{
public:
    ServerPacketHandler() = default;

    void SetService(NetService* service) { _service = service; }
    bool HandlePacket(Session& session, const Packet& pkt) override;

private:
    bool HandleChat(Session& session, const Packet& pkt);
    bool HandlePing(Session& session, const Packet&);
    bool HandlePong(Session&, const Packet&);

private:
    NetService* _service = nullptr;
};