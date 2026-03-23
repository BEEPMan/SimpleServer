#pragma once
#include <WinSock2.h>
#include <windows.h>
#include <vector>
#include <deque>
#include <mutex>
#include <atomic>

#include "IoContext.h"

class IPacketHandler;
class NetService;

class Session
{
public:
    Session(SOCKET s, HANDLE iocp, IPacketHandler& packetHandler, NetService& owner);
    ~Session();

    SOCKET GetSocket() const { return _sock; }
    bool IsClosing() const { return _closing.load(std::memory_order_acquire); }

    void Start();

    void OnRecvComplete(const char* data, int len);
    void OnSendComplete();

    void EnqueueSend(const SendBufferRef& sendBuffer);
    void EnqueueSend(std::vector<char>&& packet);

    void Close();

    void AddRef();
    void Release();

    NetService& GetService() { return _owner; }

private:
    void PostRecv();
    void PostSendFront_NoLock();

private:
    SOCKET _sock = INVALID_SOCKET;
    HANDLE _iocp = nullptr;
    IPacketHandler& _packetHandler;
    NetService& _owner;

    std::atomic_long _refCount{ 1 };
    std::atomic_bool _closing{ false };

    std::vector<char> _recvStream;

    std::mutex _sendLock;
    std::deque<SendBufferRef> _sendQ;
    bool _sending = false;
};