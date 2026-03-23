#include "Session.h"
#include "IPacketHandler.h"
#include "Packet.h"
#include "NetService.h"

Session::Session(SOCKET s, HANDLE iocp, IPacketHandler& packetHandler, NetService& owner)
    : _sock(s), _iocp(iocp), _packetHandler(packetHandler), _owner(owner)
{
    _recvStream.reserve(8192);
}

Session::~Session()
{
    if (_sock != INVALID_SOCKET)
        closesocket(_sock);
}

void Session::Start()
{
    _owner.NotifyConnected(*this);
    PostRecv();
}

void Session::Close()
{
    bool expected = false;
    if (!_closing.compare_exchange_strong(expected, true))
        return;

    if (_sock != INVALID_SOCKET)
    {
        shutdown(_sock, SD_BOTH);
        closesocket(_sock);
        _sock = INVALID_SOCKET;
    }

    _owner.NotifyDisconnected(*this);
    Release();
}

void Session::AddRef()
{
    _refCount.fetch_add(1, std::memory_order_relaxed);
}

void Session::Release()
{
    if (_refCount.fetch_sub(1, std::memory_order_acq_rel) == 1)
        delete this;
}

void Session::PostRecv()
{
    if (_closing.load(std::memory_order_acquire))
        return;

    AddRef();

    auto* ctx = new IoContext(IOType::RECV);

    DWORD flags = 0;
    DWORD bytes = 0;
    int ret = WSARecv(_sock, &ctx->wsaBuf, 1, &bytes, &flags, &ctx->overlapped, nullptr);

    if (ret == SOCKET_ERROR)
    {
        const int err = WSAGetLastError();
        if (err != WSA_IO_PENDING)
        {
            delete ctx;
            Release();
            Close();
        }
    }
}

void Session::EnqueueSend(const SendBufferRef& sendBuffer)
{
    if (!sendBuffer || sendBuffer->Size() <= 0)
        return;

    if (_closing.load(std::memory_order_acquire))
        return;

    std::lock_guard<std::mutex> lock(_sendLock);
    _sendQ.push_back(sendBuffer);

    if (_sending)
        return;

    _sending = true;
    PostSendFront_NoLock();
}

void Session::EnqueueSend(std::vector<char>&& packet)
{
    auto sendBuffer = MakeSendBuffer(packet);
    EnqueueSend(sendBuffer);
}

void Session::PostSendFront_NoLock()
{
    if (_sendQ.empty() || _closing.load(std::memory_order_acquire))
    {
        _sending = false;
        return;
    }

    AddRef();

    auto* ctx = new IoContext(IOType::SEND);
    ctx->BindSendBuffer(_sendQ.front());

    DWORD bytes = 0;
    int ret = WSASend(_sock, &ctx->wsaBuf, 1, &bytes, 0, &ctx->overlapped, nullptr);

    if (ret == SOCKET_ERROR)
    {
        const int err = WSAGetLastError();
        if (err != WSA_IO_PENDING)
        {
            delete ctx;
            Release();
            Close();
        }
    }
}

void Session::OnSendComplete()
{
    std::lock_guard<std::mutex> lock(_sendLock);

    if (!_sendQ.empty())
        _sendQ.pop_front();

    if (_sendQ.empty() || _closing.load(std::memory_order_acquire))
    {
        _sending = false;
        return;
    }

    PostSendFront_NoLock();
}

void Session::OnRecvComplete(const char* data, int len)
{
    if (_closing.load(std::memory_order_acquire))
        return;

    _recvStream.insert(_recvStream.end(), data, data + len);

    while (true)
    {
        Packet pkt{};
        const auto result = TryGetPacket(_recvStream, pkt);

        if (result == PacketReadResult::Incomplete)
            break;

        if (result != PacketReadResult::Success)
        {
            Close();
            return;
        }

        if (!_packetHandler.HandlePacket(*this, pkt))
        {
            Close();
            return;
        }

        if (_closing.load(std::memory_order_acquire))
            return;
    }

    PostRecv();
}