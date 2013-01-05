#pragma once

#include <string>
#include <tuple>

class UdpData;

class Udp {
public:
    Udp();
    ~Udp();
    
    void bind(const std::string& address, int port);

    std::tuple<std::string, int, std::string> recv();
    void send(const std::string& address, int port, const std::string& data);
    void send(const std::tuple<std::string, int>& destination, const std::string& data) {
        send(std::get<0>(destination), std::get<1>(destination), data);
    }
private:
    UdpData *d;
};
