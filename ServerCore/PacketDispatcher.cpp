#include "PacketDispatcher.h"
#include "Packet.h"
#include "Session.h"

void PacketDispatcher::Register(uint16_t opcode, Handler handler)
{
    _handlers[opcode] = std::move(handler);
}

bool PacketDispatcher::Dispatch(Session& session, const Packet& pkt) const
{
    auto it = _handlers.find(pkt.header.opcode);
    if (it == _handlers.end())
        return false;

    it->second(session, pkt);
    return true;
}