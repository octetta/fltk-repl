#include "UdpBridge.h"
#include "repl/repl_api.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <iostream>

#ifndef _WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#else
#include <winsock2.h>
#include <ws2tcpip.h>
typedef int socklen_t;
#endif

static void udp_read_cb(int fd, void *userdata) {
    (void)fd;
    UdpBridge *b = (UdpBridge *)userdata;
    if (b) b->handleRead();
}

UdpBridge::UdpBridge(repl_ctx *ctx) : ctx_(ctx) {}

UdpBridge::~UdpBridge() {
    disconnectTarget();
}

bool UdpBridge::connectTarget(const std::string &host, int port) {
    disconnectTarget();

    if (host.empty() || port <= 0 || port > 65535) return false;

#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) return false;

    // Set non-blocking socket
#ifndef _WIN32
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#else
    u_long mode = 1;
    ioctlsocket(fd, FIONBIO, &mode);
#endif

    // Bind to ephemeral port
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = 0;

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
#ifndef _WIN32
        close(fd);
#else
        closesocket(fd);
#endif
        return false;
    }

    socklen_t len = sizeof(addr);
    if (getsockname(fd, (struct sockaddr *)&addr, &len) == 0) {
        localPort_ = ntohs(addr.sin_port);
    }

    fd_ = fd;
    targetHost_ = host;
    targetPort_ = port;
    sentCount_ = 0;
    recvCount_ = 0;

    repl_add_fd(fd_, udp_read_cb, this);
    return true;
}

void UdpBridge::disconnectTarget() {
    if (fd_ >= 0) {
        repl_remove_fd(fd_);
#ifndef _WIN32
        close(fd_);
#else
        closesocket(fd_);
#endif
        fd_ = -1;
    }
    targetHost_.clear();
    targetPort_ = 0;
    localPort_ = 0;
}

bool UdpBridge::sendData(const char *data, size_t len) {
    if (!isConnected() || !data || len == 0) return false;

    struct sockaddr_in target_addr;
    memset(&target_addr, 0, sizeof(target_addr));
    target_addr.sin_family = AF_INET;
    target_addr.sin_port = htons((uint16_t)targetPort_);
    if (inet_pton(AF_INET, targetHost_.c_str(), &target_addr.sin_addr) <= 0) {
        return false;
    }

    ssize_t n = sendto(fd_, data, len, 0, (struct sockaddr *)&target_addr, sizeof(target_addr));
    if (n > 0) {
        sentCount_++;
        return true;
    }
    return false;
}

void UdpBridge::handleRead() {
    if (fd_ < 0 || !ctx_) return;

    char buf[4096];
    struct sockaddr_in src_addr;
    socklen_t addr_len = sizeof(src_addr);

    ssize_t n = recvfrom(fd_, buf, sizeof(buf) - 1, 0, (struct sockaddr *)&src_addr, &addr_len);
    if (n > 0) {
        buf[n] = '\0';
        recvCount_++;

        char sender_ip[INET_ADDRSTRLEN] = {0};
        inet_ntop(AF_INET, &src_addr.sin_addr, sender_ip, sizeof(sender_ip));
        int sender_port = ntohs(src_addr.sin_port);

        std::string formatted;
        char prefix[128];
        snprintf(prefix, sizeof(prefix), "\x1b[38;5;%dm[udp %s:%d] ", colorCode_, sender_ip, sender_port);
        formatted += prefix;
        formatted += buf;
        formatted += "\x1b[0m";
        if (formatted.back() != '\n') formatted += "\n";

        repl_print(ctx_, formatted.c_str());
    }
}
