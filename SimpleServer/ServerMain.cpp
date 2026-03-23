#include <WinSock2.h>
#include <WS2tcpip.h>
#include <iostream>

#include "NetService.h"
#include "ServerPacketHandler.h"
#include "Session.h"

#pragma comment(lib, "ws2_32.lib")

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

    service.SetOnConnected([](Session&)
        {
            std::cout << "[Server] Client connected\n";
        });

    service.SetOnDisconnected([](Session&)
        {
            std::cout << "[Server] Client disconnected\n";
        });

    if (!service.StartServer("0.0.0.0", 7777, 4))
    {
        std::cout << "Failed to start server\n";
        WSACleanup();
        return 1;
    }

    service.RunAcceptLoop();

    service.Stop();
    WSACleanup();
    return 0;
}