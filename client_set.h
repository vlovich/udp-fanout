#pragma once

#include <unordered_set>
#include <tuple>
#include <string>
#include <mutex>
#include <vector>

using Client = std::tuple<std::string, int>;

namespace std {
    template <>
    struct hash<Client> {
        size_t operator()(const Client& client) const {
            return hash<string>()(get<0>(client)) * 31 +
                   hash<int>()(get<1>(client));
        }
    };
}

class ClientSet {
public:
    using scoped_lock = std::lock_guard<std::mutex>;

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

