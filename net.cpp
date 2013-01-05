#include "net.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>

#include <stdexcept>
#include <array>
#include <memory>
#include <cstdlib>

//#ifndef AI_NUMERICSERV
//#define AI_NUMERICSERV 0
//#endif

constexpr int UDP_BUFLEN = 65536;
constexpr int ADDRSTRLEN = 50;

template <class Ex>
void raise_exception(const std::string& msg) {
    perror(msg.c_str());
    throw Ex(msg);
}

#define raise_io_exception raise_exception<std::runtime_error>

using SockAddrPtr = std::unique_ptr<sockaddr, void (*)(void *)>;
using SockAddrInfo = std::tuple<SockAddrPtr, size_t, int>;

static SockAddrInfo convert(const std::string &address, const std::string& service) {
    struct addrinfo *resolved;
    struct addrinfo hints;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_NUMERICSERV;

    int err = getaddrinfo(address.c_str(), service.c_str(), &hints, &resolved);
    if (err != 0) {
        raise_io_exception("Failed to resolve '" + address + "': " + std::string(gai_strerror(err)));
    }

    SockAddrPtr addr((sockaddr *)std::malloc(sizeof(resolved->ai_addrlen)), std::free);
    memcpy(addr.get(), resolved->ai_addr, resolved->ai_addrlen);
    auto result = std::make_tuple(std::move(addr), resolved->ai_addrlen, resolved->ai_family);

    freeaddrinfo(resolved);

    return std::move(result);
}

template <class Dest, class Src>
static Dest lexical_cast(const Src& from);

template <>
std::string lexical_cast<std::string, int>(const int& i) {
    char buffer[13];
    snprintf(buffer, sizeof(buffer), "%d", i);
    return buffer;
}

class UdpData {
public:
    UdpData() {
        _socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (_socket == -1) {
            raise_io_exception("Failed to create UDP socket");
        }
    }

    void bind(const std::string& address, int port) {
        auto bindinfo = convert(address, lexical_cast<std::string>(port));

        int r = ::bind(_socket, std::get<0>(bindinfo).get(), std::get<1>(bindinfo));
        if (r == -1) {
            raise_io_exception("Failed to bind to " + address);
        }
        
        recvbuffer.reserve(UDP_BUFLEN);
    }

    std::tuple<std::string, int, std::string> recv() {
        struct sockaddr_in sender_info;
        socklen_t sender_info_len;

        recvbuffer.resize(UDP_BUFLEN);
        ssize_t nbytes = ::recvfrom(_socket, const_cast<char *>(recvbuffer.data()), recvbuffer.capacity(), 0 /* flags */, (sockaddr *)&sender_info, &sender_info_len);
        if (nbytes == -1) {
            raise_io_exception("Failed to receive data");
	}
        recvbuffer.resize(nbytes);

        char sender_address_buf[ADDRSTRLEN];
        std::string sender_address = inet_ntop(AF_INET, &(sender_info.sin_addr), sender_address_buf, sizeof(sender_address_buf));

        return std::make_tuple(sender_address, ntohs(sender_info.sin_port), recvbuffer);
    }

    void send(const std::string& address, int port, const std::string& data) {
	auto destinfo = convert(address, lexical_cast<std::string>(port));
	int sent = ::sendto(_socket, data.c_str(), data.size(), 0 /* flags */, std::get<0>(destinfo).get(), std::get<1>(destinfo));
	if (sent != data.size()) {
	    raise_io_exception("Failed to send data to '" + address + "'");
	}
    }

    ~UdpData() {
        close(_socket);
    }
private:
    int _socket;
    std::string recvbuffer;
};

Udp::Udp()
    : d(new UdpData())
{
}

Udp::~Udp() {
    delete d;
}

void Udp::bind(const std::string& address, int port) {
    d->bind(address, port);
}

std::tuple<std::string, int, std::string> Udp::recv() {
    return d->recv();
}

void Udp::send(const std::string& address, int port, const std::string& data) {
    return d->send(address, port, data);
}

