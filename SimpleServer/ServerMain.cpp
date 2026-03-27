#include <WinSock2.h>
#include <iostream>
#include <atomic>

#include "NetService.h"
#include "ServerPacketHandler.h"
#include "ClientSession.h"
#include "Player.h"

#pragma comment(lib, "ws2_32.lib")

#ifdef _DEBUG
#pragma comment(lib, "Debug\\libprotobufd.lib")
#else
#pragma comment(lib, "Release\\libprotobuf.lib")
#endif

static std::atomic<uint64_t> s_nextPlayerId{ 1 };

int main()
{
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        std::cout << "WSAStartup failed\n";
        return 1;
    }

    ServerPacketHandler handler;
    NetService service(handler);
    handler.SetService(&service);

    service.SetSessionFactory([](SOCKET sock, IPacketHandler& h, NetService& svc) -> Session*
        {
            return new ClientSession(sock, h, svc);
        });

    service.SetOnConnected([](Session& session)
        {
            auto& cs = static_cast<ClientSession&>(session);
            uint64_t id = s_nextPlayerId.fetch_add(1, std::memory_order_relaxed);
            cs.SetPlayer(std::make_shared<Player>(id, "플레이어_" + std::to_string(id)));
            std::cout << "[Server] Client connected. Player id=" << id << "\n";
        });

    service.SetOnDisconnected([](Session& session)
        {
            auto& cs = static_cast<ClientSession&>(session);
            auto player = cs.GetPlayer();
            std::cout << "[Server] Client disconnected. Player id=" << (player ? player->GetPlayerId() : 0) << "\n";
        });

    if (!service.StartServer("0.0.0.0", 7777, 4))
    {
        std::cout << "Server start failed\n";
        WSACleanup();
        return 1;
    }

    service.RunAcceptLoop();

    service.Stop();
    WSACleanup();
    return 0;
}
