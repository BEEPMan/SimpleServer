#pragma once
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <windows.h>
#include <cstdint>
#include <memory>
#include <cstring>
#include "SendBuffer.h"

constexpr int RECV_POST_SIZE = 4096;

enum class IOType : uint8_t
{
    RECV,
    SEND
};

struct IoContext
{
    OVERLAPPED overlapped{};
    WSABUF wsaBuf{};
    IOType type{};

    char recvBuffer[RECV_POST_SIZE]{};

    SendBufferRef sendBuffer;

    explicit IoContext(IOType t) : type(t)
    {
        ZeroMemory(&overlapped, sizeof(overlapped));

        if (type == IOType::RECV)
        {
            wsaBuf.buf = recvBuffer;
            wsaBuf.len = RECV_POST_SIZE;
        }
        else
        {
            wsaBuf.buf = nullptr;
            wsaBuf.len = 0;
        }
    }

    void BindSendBuffer(const SendBufferRef& buffer)
    {
        sendBuffer = buffer;
        wsaBuf.buf = sendBuffer ? sendBuffer->Data() : nullptr;
        wsaBuf.len = sendBuffer ? static_cast<ULONG>(sendBuffer->Size()) : 0;
    }
};