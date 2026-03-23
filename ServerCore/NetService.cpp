#include "NetService.h"
#include "Session.h"
#include "IPacketHandler.h"
#include "IoContext.h"
#include "Packet.h"

#include <WS2tcpip.h>
#include <iostream>
#include <cstring>

#pragma comment(lib, "ws2_32.lib")

NetService::NetService(IPacketHandler& packetHandler)
    : _packetHandler(packetHandler)
{
}

NetService::~NetService()
{
    Stop();
}

bool NetService::InitIocpAndWorkers(int workerCount)
{
    _iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
    if (!_iocp)
        return false;

    _workers.reserve(workerCount);
    for (int i = 0; i < workerCount; ++i)
    {
        HANDLE h = CreateThread(nullptr, 0, WorkerThunk, this, 0, nullptr);
        if (!h)
            return false;
        _workers.push_back(h);
    }

    return true;
}

bool NetService::StartServer(const char* ip, uint16_t port, int workerCount)
{
    if (_running.exchange(true))
        return false;

    _mode = Mode::Server;

    _listen = socket(AF_INET, SOCK_STREAM, 0);
    if (_listen == INVALID_SOCKET)
        return false;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (!ip || std::strcmp(ip, "0.0.0.0") == 0)
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
    else
        inet_pton(AF_INET, ip, &addr.sin_addr);

    int opt = 1;
    setsockopt(_listen, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));

    if (bind(_listen, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR)
        return false;

    if (listen(_listen, SOMAXCONN) == SOCKET_ERROR)
        return false;

    if (!InitIocpAndWorkers(workerCount))
        return false;

    std::cout << "[Server] Started on " << (ip ? ip : "0.0.0.0") << ":" << port << "\n";
    return true;
}

bool NetService::StartClient(int workerCount)
{
    if (_running.exchange(true))
        return false;

    _mode = Mode::Client;

    if (!InitIocpAndWorkers(workerCount))
        return false;

    std::cout << "[ClientRuntime] Started\n";
    return true;
}

bool NetService::Connect(const char* ip, uint16_t port)
{
    if (_mode != Mode::Client || !_running.load())
        return false;

    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET)
        return false;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &addr.sin_addr);

    if (connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR)
    {
        closesocket(sock);
        return false;
    }

    auto* session = new Session(sock, _iocp, _packetHandler, *this);

    HANDLE h = CreateIoCompletionPort(reinterpret_cast<HANDLE>(sock), _iocp, reinterpret_cast<ULONG_PTR>(session), 0);
    if (!h)
    {
        session->Close();
        return false;
    }

    AddSession(session);
    session->Start();
    return true;
}

void NetService::RunAcceptLoop()
{
    if (_mode != Mode::Server)
        return;

    while (_running.load(std::memory_order_acquire))
    {
        SOCKET client = accept(_listen, nullptr, nullptr);
        if (client == INVALID_SOCKET)
        {
            if (_running.load())
                std::cout << "[Server] accept failed: " << WSAGetLastError() << "\n";
            continue;
        }

        auto* session = new Session(client, _iocp, _packetHandler, *this);

        HANDLE h = CreateIoCompletionPort(reinterpret_cast<HANDLE>(client), _iocp, reinterpret_cast<ULONG_PTR>(session), 0);
        if (!h)
        {
            session->Close();
            continue;
        }

        AddSession(session);
        session->Start();
    }
}

void NetService::Stop()
{
    if (!_running.exchange(false))
        return;

    if (_listen != INVALID_SOCKET)
    {
        closesocket(_listen);
        _listen = INVALID_SOCKET;
    }

    std::vector<Session*> snapshot;
    {
        std::lock_guard<std::mutex> lock(_sessionLock);
        snapshot.assign(_sessions.begin(), _sessions.end());
    }

    for (Session* session : snapshot)
    {
        if (session)
            session->Close();
    }

    if (_iocp)
    {
        for (size_t i = 0; i < _workers.size(); ++i)
            PostQueuedCompletionStatus(_iocp, 0, 0, nullptr);

        if (!_workers.empty())
            WaitForMultipleObjects(static_cast<DWORD>(_workers.size()), _workers.data(), TRUE, INFINITE);

        for (HANDLE h : _workers)
            CloseHandle(h);

        _workers.clear();

        CloseHandle(_iocp);
        _iocp = nullptr;
    }
}

void NetService::Broadcast(const SendBufferRef& sendBuffer)
{
    if (!sendBuffer)
        return;

    std::vector<Session*> snapshot;
    {
        std::lock_guard<std::mutex> lock(_sessionLock);
        snapshot.assign(_sessions.begin(), _sessions.end());
    }

    for (Session* session : snapshot)
    {
        if (session && !session->IsClosing())
            session->EnqueueSend(sendBuffer);
    }
}

void NetService::Broadcast(std::vector<char>&& packet)
{
    auto sendBuffer = MakeSendBuffer(packet);
    Broadcast(sendBuffer);
}

void NetService::NotifyConnected(Session& session)
{
    if (_onConnected)
        _onConnected(session);
}

void NetService::NotifyDisconnected(Session& session)
{
    RemoveSession(&session);

    if (_onDisconnected)
        _onDisconnected(session);
}

void NetService::AddSession(Session* session)
{
    std::lock_guard<std::mutex> lock(_sessionLock);
    _sessions.insert(session);
}

void NetService::RemoveSession(Session* session)
{
    std::lock_guard<std::mutex> lock(_sessionLock);
    _sessions.erase(session);
}

DWORD WINAPI NetService::WorkerThunk(LPVOID p)
{
    static_cast<NetService*>(p)->WorkerLoop();
    return 0;
}

void NetService::WorkerLoop()
{
    while (true)
    {
        DWORD bytes = 0;
        ULONG_PTR key = 0;
        LPOVERLAPPED pov = nullptr;

        BOOL ok = GetQueuedCompletionStatus(_iocp, &bytes, &key, &pov, INFINITE);

        if (key == 0 && pov == nullptr)
            break;

        auto* session = reinterpret_cast<Session*>(key);
        auto* ctx = reinterpret_cast<IoContext*>(pov);

        if (!ok || bytes == 0)
        {
            if (session) session->Close();
            delete ctx;
            if (session) session->Release();
            continue;
        }

        if (ctx->type == IOType::RECV)
            session->OnRecvComplete(ctx->recvBuffer, static_cast<int>(bytes));
        else
            session->OnSendComplete();

        delete ctx;
        session->Release();
    }
}