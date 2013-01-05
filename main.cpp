#include <unordered_set>
#include <thread>
#include <iostream>
#include <vector>
#include <mutex>

#include "net.h"

using Client = std::tuple<std::string, int>;
using scoped_lock = std::lock_guard<std::mutex>;

namespace std {
    template <>
    struct hash<Client> {
        size_t operator()(const Client& client) const {
            return hash<string>()(get<0>(client)) * 31 +
                   hash<int>()(get<1>(client));
        }
    };
}

class LockedOStream {
public:
    LockedOStream(std::ostream& ostream, std::mutex& ostream_mutex) 
        : ostream(ostream)
        , ostream_lock(ostream_mutex)
    {
    }
    
    template <class T>
    LockedOStream& operator<<(const T& data) {
        ostream << data;
        return *this;
    }
    
private:
    std::ostream& ostream;
    scoped_lock ostream_lock;
};

class ClientSet {
public:
    ClientSet() {
    }
    
    ~ClientSet() {
    }
    
    bool addClient(const std::string& address, int port) {
        return addClient(std::tie(address, port));
    }
    
    bool addClient(const Client& client) {
        scoped_lock lock(clientsMutex);
        return clients.insert(client).second;
    }
    
    bool removeClient(const std::string& address, int port) {
        return removeClient(std::tie(address, port));
    }
    
    bool removeClient(const Client& client) {
        scoped_lock lock(clientsMutex);
        return clients.erase(client) > 0;
    }
    
    std::vector<Client> getClientList() const {
        scoped_lock lock(clientsMutex);
        std::vector<Client> result;
        result.reserve(clients.size());
        for (auto i = clients.begin(), ni = clients.end(); i != ni; i++) {
            result.push_back(*i);
        }
        return result;
    }

private:
    mutable std::mutex clientsMutex;
    std::unordered_set<Client> clients;
};

static std::mutex console_lock;

/**
 * Any packet received on this port registers the
 * sender as a target for outflows.
 */
static void outflow_admin(int port, ClientSet *registrations) {
    Udp listener;
    listener.bind("0.0.0.0", port);

    LockedOStream(std::cout, console_lock) << "Waiting for clients to register on " << port << "\n";

    while (true) {
        std::string client;
        int port;
        std::string data;
        std::tie(client, port, data) = listener.recv();
        if (data == "unregister") {
            if (registrations->removeClient(client, port)) {
                LockedOStream(std::cout, console_lock) << "Unregistered " << client << ":" << port << "\n";
            } else {
                LockedOStream(std::cerr, console_lock) << "Client " << client << ":" << port << " not registered or already unregistered\n";
            }
        } else if (data == "register") {
            if (registrations->addClient(client, port)) {
                LockedOStream(std::cout, console_lock) << "Registered client " << client << ":" << port << ".  Payload (" << data << ")\n";
            } else {
                LockedOStream(std::cerr, console_lock) << "Client " << client << ":" << port << " already registered\n";
            }
        } else {
            LockedOStream(std::cerr, console_lock) << "Unsupported command (" << data << ") from client " << client << ":" << port << "\n";
        }
    }
}

static void udp_mirror(int incoming_port, ClientSet *registrations) {
    Udp listener;
    Udp mirror;

    listener.bind("0.0.0.0", incoming_port);

    LockedOStream(std::cout, console_lock) << "Listening for incoming UDP data on " << incoming_port << "\n";

    while (true) {
        std::string sender;
        int sender_port;
        std::string data;
        std::tie(sender, sender_port, data) = listener.recv();

        auto registered = registrations->getClientList();

        for (const auto& client : registered) {
            // TODO: spoof src IP/PORT to be sender/sender_port
            mirror.send(std::get<0>(client), std::get<1>(client), data);
        }

        LockedOStream(std::cout, console_lock) << "Re-broadcast datagram " << data.size() << " bytes to " << registered.size() << " clients\n";
    }
}

int main(int argc, char **argv) {
    int incoming_port = 514;
    int outflow_admin_port = 10000;
    ClientSet registrations;
    
    std::thread mirror_thread(udp_mirror, incoming_port, &registrations);
    mirror_thread.detach();

    std::thread admin_thread(outflow_admin, outflow_admin_port, &registrations);
    admin_thread.join();
}

