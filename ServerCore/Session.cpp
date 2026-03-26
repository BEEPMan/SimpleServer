#include "Session.h"
#include "IPacketHandler.h"
#include "Packet.h"
#include "NetService.h"
#include <iostream>

Session::Session(SOCKET s, IPacketHandler& packetHandler, NetService& owner)
    : _sock(s), _packetHandler(packetHandler), _owner(owner)
{
    _recvStream.reserve(MAX_RECV_STREAM_SIZE);
}

Session::~Session()
{
    if (_sock != INVALID_SOCKET)
        closesocket(_sock);
}

void Session::Start()
{
    std::cout << "[Session] connected\n";
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

    {
        std::lock_guard<std::mutex> lock(_sendLock);
        _sendQ.clear();
        _sending = false;
    }

    _recvStream.clear();
    _recvStream.shrink_to_fit();

    std::cout << "[Session] disconnected\n";
    _owner.NotifyDisconnected(*this);
    Release();
}

void Session::CloseWithLog(const char* reason)
{
    std::cout << "[Session] " << reason << " — closing\n";
    Close();
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

    auto ctx = std::make_unique<IoContext>(IOType::RECV);

    DWORD flags = 0;
    DWORD bytes = 0;
    int ret = WSARecv(_sock, &ctx->wsaBuf, 1, &bytes, &flags, &ctx->overlapped, nullptr);

    if (ret == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
    {
        Release();
        Close();
        return;
    }

    ctx.release();
}

bool Session::PostSendFront_NoLock()
{
    if (_sendQ.empty() || _closing.load(std::memory_order_acquire))
    {
        _sending = false;
        return true;
    }

    AddRef();

    auto ctx = std::make_unique<IoContext>(IOType::SEND);
    ctx->BindSendBuffer(_sendQ.front());

    DWORD bytes = 0;
    int ret = WSASend(_sock, &ctx->wsaBuf, 1, &bytes, 0, &ctx->overlapped, nullptr);

    if (ret == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
    {
        Release();
        _sending = false;
        return false;
    }

    ctx.release();
    return true;
}

void Session::EnqueueSend(const SendBufferRef& sendBuffer)
{
    if (!sendBuffer || sendBuffer->Size() <= 0)
        return;

    if (_closing.load(std::memory_order_acquire))
        return;

    bool needClose = false;
    {
        std::lock_guard<std::mutex> lock(_sendLock);

        if (_sendQ.size() >= MAX_SEND_QUEUE_SIZE)
        {
            needClose = true;
        }
        else
        {
            _sendQ.push_back(sendBuffer);

            if (!_sending)
            {
                _sending = true;
                if (!PostSendFront_NoLock())
                    needClose = true;
            }
        }
    }

    if (needClose)
        CloseWithLog("송신 큐 한계 초과");
}

void Session::EnqueueSend(std::vector<char>&& packet)
{
    auto sendBuffer = MakeSendBuffer(packet);
    EnqueueSend(sendBuffer);
}

void Session::OnSendComplete()
{
    bool needClose = false;
    {
        std::lock_guard<std::mutex> lock(_sendLock);

        if (!_sendQ.empty())
            _sendQ.pop_front();

        if (_sendQ.empty() || _closing.load(std::memory_order_acquire))
        {
            _sending = false;
            return;
        }

        if (!PostSendFront_NoLock())
            needClose = true;
    }

    if (needClose)
        CloseWithLog("송신 실패");
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

        if (result == PacketReadResult::InvalidHeader)
        {
            CloseWithLog("잘못된 패킷 헤더");
            return;
        }

        if (result == PacketReadResult::PacketTooLarge)
        {
            CloseWithLog("패킷 크기 초과");
            return;
        }

        if (result != PacketReadResult::Success)
        {
            CloseWithLog("알 수 없는 패킷 오류");
            return;
        }

        if (!_packetHandler.HandlePacket(*this, pkt))
        {
            CloseWithLog("핸들러 처리 실패");
            return;
        }

        if (_closing.load(std::memory_order_acquire))
            return;
    }

    PostRecv();
}
