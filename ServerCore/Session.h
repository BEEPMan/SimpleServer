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
    Session(SOCKET s, IPacketHandler& packetHandler, NetService& owner);
    virtual ~Session();

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
    static constexpr size_t MAX_SEND_QUEUE_SIZE = 64;

    void PostRecv();
    bool PostSendFront_NoLock(); // true = 성공(IO_PENDING 포함), false = WSASend 실패
    void CloseWithLog(const char* reason);

private:
    SOCKET _sock = INVALID_SOCKET;
    IPacketHandler& _packetHandler;
    NetService& _owner;

    std::atomic_long _refCount{ 1 };
    std::atomic_bool _closing{ false };

    std::vector<char> _recvStream;

    std::mutex _sendLock;
    std::deque<SendBufferRef> _sendQ;
    bool _sending = false;
};
