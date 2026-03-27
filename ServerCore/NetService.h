#pragma once
#include <WinSock2.h>
#include <windows.h>
#include <vector>
#include <atomic>
#include <unordered_set>
#include <mutex>
#include <functional>

#include "SendBuffer.h"

class Session;
class IPacketHandler;

class NetService
{
public:
    enum class Mode : uint8_t
    {
        None,
        Server,
        Client
    };

public:
    explicit NetService(IPacketHandler& packetHandler);
    ~NetService();

    bool StartServer(const char* ip, uint16_t port, int workerCount = 4);
    bool StartClient(int workerCount = 1);
    bool Connect(const char* ip, uint16_t port);

    void RunAcceptLoop();
    void Stop();

    void Broadcast(const SendBufferRef& sendBuffer);
    void Broadcast(std::vector<char>&& packet);

    using SessionFactory = std::function<Session*(SOCKET, IPacketHandler&, NetService&)>;
    void SetSessionFactory(SessionFactory factory) { _sessionFactory = std::move(factory); }

    void ForEachSession(std::function<void(Session&)> fn);

    void SetOnConnected(std::function<void(Session&)> fn) { _onConnected = std::move(fn); }
    void SetOnDisconnected(std::function<void(Session&)> fn) { _onDisconnected = std::move(fn); }

    void NotifyConnected(Session& session);
    void NotifyDisconnected(Session& session);

private:
    static DWORD WINAPI WorkerThunk(LPVOID p);
    void WorkerLoop();

    bool InitIocpAndWorkers(int workerCount);
    bool CreateAndRegisterSession(SOCKET sock);
    void AddSession(Session* session);
    void RemoveSession(Session* session);

private:
    IPacketHandler& _packetHandler;
    Mode _mode = Mode::None;

    SOCKET _listen = INVALID_SOCKET;
    HANDLE _iocp = nullptr;
    std::vector<HANDLE> _workers;
    std::atomic_bool _running{ false };

    std::mutex _sessionLock;
    std::unordered_set<Session*> _sessions;

    SessionFactory                _sessionFactory;
    std::function<void(Session&)> _onConnected;
    std::function<void(Session&)> _onDisconnected;
};