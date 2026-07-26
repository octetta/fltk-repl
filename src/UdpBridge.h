#pragma once

#include <string>
#include <cstdint>

struct repl_ctx;

class UdpBridge {
public:
    enum Mode { MODE_FORWARD, MODE_LOG, MODE_OFF };

    UdpBridge(repl_ctx *ctx);
    ~UdpBridge();

    bool connectTarget(const std::string &host, int port);
    void disconnectTarget();
    bool sendData(const char *data, size_t len);

    bool isConnected() const { return fd_ >= 0 && targetPort_ > 0; }

    const std::string &targetHost() const { return targetHost_; }
    int targetPort() const { return targetPort_; }
    int localPort() const { return localPort_; }

    void setColor(int ansi_code) { colorCode_ = ansi_code; }
    int color() const { return colorCode_; }

    void setMode(Mode m) { mode_ = m; }
    Mode mode() const { return mode_; }

    uint64_t packetsSent() const { return sentCount_; }
    uint64_t packetsRecv() const { return recvCount_; }

    void handleRead();

private:
    repl_ctx *ctx_ = nullptr;
    int fd_ = -1;
    std::string targetHost_;
    int targetPort_ = 0;
    int localPort_ = 0;
    int colorCode_ = 51; // Bright Cyan
    Mode mode_ = MODE_FORWARD;
    uint64_t sentCount_ = 0;
    uint64_t recvCount_ = 0;
};
