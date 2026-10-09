#pragma once
// V1 transport only. Capture remains owned by the V0 loop; no retries or blocking sends.
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <objbase.h>
#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <string>

inline int64_t FingerQpc() noexcept {
    LARGE_INTEGER t{};
    return QueryPerformanceCounter(&t) ? t.QuadPart : 0;
}
class OscSender {
    SOCKET socket_ = INVALID_SOCKET;
    bool wsa_ = false;
    sockaddr_in destination_{};
    std::array<char,33> session_{};
    int64_t start_ = 0, frequency_ = 0;
    std::array<int64_t,2> sequence_{};
    uint64_t sent_ = 0, failed_ = 0;
    int lastError_ = 0;
    bool NewSession() noexcept {
        GUID guid{};
        if (FAILED(CoCreateGuid(&guid))) return false;
        static constexpr char hex[] = "0123456789abcdef";
        std::array<unsigned char, 16> bytes{};
        std::memcpy(bytes.data(), &guid, bytes.size());
        size_t at=0;
        for (auto b : bytes) { session_[at++]=hex[b >> 4]; session_[at++]=hex[b & 15]; }
        session_[32]=0;
        start_ = FingerQpc(); sequence_.fill(0);
        return start_ > 0;
    }
public:
    OscSender(bool enabled, int port) {
        if (!enabled) return;
        WSADATA wsa{};
        const int initialized = WSAStartup(MAKEWORD(2,2), &wsa);
        if (initialized) { std::cerr << "OSC disabled: WSAStartup=" << initialized << '\n'; return; }
        wsa_ = true;
        LARGE_INTEGER frequency{};
        if (!QueryPerformanceFrequency(&frequency) || frequency.QuadPart <= 0 || !NewSession()) {
            std::cerr << "OSC disabled: QPC/session initialization failed\n"; return;
        }
        frequency_ = frequency.QuadPart;
        socket_ = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (socket_ == INVALID_SOCKET) { std::cerr << "OSC disabled: socket error=" << WSAGetLastError() << '\n'; return; }
        u_long nonblocking = 1;
        sockaddr_in local{}; local.sin_family = AF_INET; local.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (ioctlsocket(socket_, FIONBIO, &nonblocking) || bind(socket_, reinterpret_cast<sockaddr*>(&local), sizeof(local))) {
            std::cerr << "OSC disabled: nonblocking/loopback bind error=" << WSAGetLastError() << '\n';
            closesocket(socket_); socket_ = INVALID_SOCKET; return;
        }
        destination_.sin_family = AF_INET; destination_.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        destination_.sin_port = htons(static_cast<u_short>(port));
        std::cout << "OSC V1 -> 127.0.0.1:" << port << " /mmvr/fingers/v1/{left,right} session=" << session_.data()
                  << " startQpc=" << start_ << " qpcHz=" << frequency_ << '\n';
    }
    ~OscSender() { if (socket_ != INVALID_SOCKET) closesocket(socket_); if (wsa_) WSACleanup(); }
    OscSender(const OscSender&) = delete;
    OscSender& operator=(const OscSender&) = delete;
    void BeginCycle() noexcept {
        // Rotate a common epoch before either hand is acquired. Never relabel a cached acquisition.
        if (sequence_[0] == std::numeric_limits<int64_t>::max() || sequence_[1] == std::numeric_limits<int64_t>::max()) {
            if (!NewSession()) { closesocket(socket_); socket_ = INVALID_SOCKET; ++failed_; }
        }
    }
    void Send(size_t side, bool valid, int level, const std::array<float,5>& curls, int64_t acquisition) noexcept {
        if (socket_ == INVALID_SOCKET) return;
        if (side >= sequence_.size() || sequence_[side] == std::numeric_limits<int64_t>::max()) { ++failed_; return; }
        ++sequence_[side];
        std::array<char,192> bytes{};
        size_t used = 0;
        auto string = [&](const char* value) {
            const size_t length=std::strlen(value);
            std::memcpy(bytes.data()+used, value, length+1);
            used += (length+1+3) & ~size_t(3);
        };
        auto integer = [&](uint64_t value, unsigned width) {
            for (unsigned i=0; i<width; ++i) bytes[used++] = static_cast<char>((value >> ((width-1-i)*8)) & 255);
        };
        string(side == 0 ? "/mmvr/fingers/v1/left" : "/mmvr/fingers/v1/right"); string(",shhhhiifffff"); string(session_.data());
        integer(static_cast<uint64_t>(start_),8); integer(static_cast<uint64_t>(sequence_[side]),8);
        integer(static_cast<uint64_t>(acquisition),8); integer(static_cast<uint64_t>(frequency_),8);
        integer(valid ? 1 : 0,4); integer(static_cast<uint32_t>(level),4);
        for (float v : curls) {
            if (!valid) v = 0.0f; // explicit invalid attempt carries no previous curls
            uint32_t bits{}; std::memcpy(&bits,&v,sizeof(bits)); integer(bits,4);
        }
        const int result = sendto(socket_,bytes.data(),static_cast<int>(used),0,
            reinterpret_cast<const sockaddr*>(&destination_),sizeof(destination_));
        if (result == static_cast<int>(used)) { ++sent_; lastError_ = 0; }
        else { ++failed_; lastError_ = WSAGetLastError(); } // best effort, no retry/queue
    }
    void SendSplays(size_t side, bool valid, int level, const std::array<float,4>& splays, int64_t acquisition) noexcept {
        if (socket_ == INVALID_SOCKET) return;
        if (side >= sequence_.size()) { ++failed_; return; }
        // Reuse the sequence just emitted by Send for this same acquisition.
        if (sequence_[side] <= 0) { ++failed_; return; }
        std::array<char,192> bytes{};
        size_t used = 0;
        auto string = [&](const char* value) {
            const size_t length=std::strlen(value);
            std::memcpy(bytes.data()+used, value, length+1);
            used += (length+1+3) & ~size_t(3);
        };
        auto integer = [&](uint64_t value, unsigned width) {
            for (unsigned i=0; i<width; ++i) bytes[used++] = static_cast<char>((value >> ((width-1-i)*8)) & 255);
        };
        string(side == 0 ? "/mmvr/fingers/v1/splay/left" : "/mmvr/fingers/v1/splay/right"); string(",shhhhiiffff"); string(session_.data());
        integer(static_cast<uint64_t>(start_),8); integer(static_cast<uint64_t>(sequence_[side]),8);
        integer(static_cast<uint64_t>(acquisition),8); integer(static_cast<uint64_t>(frequency_),8);
        integer(valid ? 1 : 0,4); integer(static_cast<uint32_t>(level),4);
        for (float v : splays) {
            if (!valid) v = 0.0f; // explicit invalid attempt carries no previous splays
            uint32_t bits{}; std::memcpy(&bits,&v,sizeof(bits)); integer(bits,4);
        }
        const int result = sendto(socket_,bytes.data(),static_cast<int>(used),0,
            reinterpret_cast<const sockaddr*>(&destination_),sizeof(destination_));
        if (result == static_cast<int>(used)) { ++sent_; lastError_ = 0; }
        else { ++failed_; lastError_ = WSAGetLastError(); } // best effort, no retry/queue
    }
    void Diagnostics() const {
        if (wsa_) std::cout << " oscSent=" << sent_ << " oscFailed=" << failed_ << " oscError=" << lastError_;
    }
};
