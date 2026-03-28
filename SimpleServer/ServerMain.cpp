#include <WinSock2.h>
#include <iostream>
#include <atomic>

#include "NetService.h"
#include "ServerPacketHandler.h"
#include "ClientSession.h"
#include "Player.h"
#include "GameRoom.h"

#pragma comment(lib, "ws2_32.lib")

static std::atomic<uint64_t> s_nextPlayerId{ 1 };

int main()
{
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        std::cout << "WSAStartup failed\n";
        return 1;
    }

    GameRoom room;
    ServerPacketHandler handler;
    NetService service(handler);
    handler.SetRoom(&room);

    service.SetSessionFactory([](SOCKET sock, IPacketHandler& h, NetService& svc) -> Session*
        {
            return new ClientSession(sock, h, svc);
        });

    service.SetOnConnected([&room](Session& session)
        {
            auto& cs = static_cast<ClientSession&>(session);
            uint64_t id = s_nextPlayerId.fetch_add(1, std::memory_order_relaxed);
            auto player = std::make_shared<Player>(id, "");
            player->SetSession(&session);
            cs.SetPlayer(player);
            room.Enter(player);
            std::cout << "[Server] Client connected. Player id=" << id << "\n";
        });

    service.SetOnDisconnected([&room](Session& session)
        {
            auto& cs = static_cast<ClientSession&>(session);
            auto player = cs.GetPlayer();
            if (player)
            {
                room.Leave(player->GetPlayerId());
                player->SetSession(nullptr);
            }
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
