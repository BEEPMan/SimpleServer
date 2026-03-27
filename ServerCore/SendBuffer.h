#pragma once
#include <vector>
#include <cstdint>
#include <stdexcept>
#include <memory>

class SendBuffer
{
public:
    explicit SendBuffer(int32_t size)
        : _buffer(size), _writeSize(0)
    {
    }

    char* Data() { return _buffer.data(); }
    const char* Data() const { return _buffer.data(); }

    int32_t Capacity() const { return static_cast<int32_t>(_buffer.size()); }
    int32_t Size() const { return _writeSize; }

    void CopyFrom(const void* src, int32_t len)
    {
        if (len > Capacity())
            throw std::runtime_error("SendBuffer overflow");

        if (len > 0)
            std::memcpy(_buffer.data(), src, len);

        _writeSize = len;
    }

private:
    std::vector<char> _buffer;
    int32_t _writeSize;
};

using SendBufferRef = std::shared_ptr<SendBuffer>;