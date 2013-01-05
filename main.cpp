#include <thread>

#include "net.h"
#include "locked_ostream.h"
#include "client_set.h"


static std::mutex console_lock;

#define cout_locked() LockedOStream(std::cout, console_lock)
#define cerr_locked() LockedOStream(std::cerr, console_lock)

/**
 * Any packet received on this port registers the
 * sender as a target for outflows.
 */
static void outflow_admin(int port, ClientSet *registrations) {
    try {
        Udp listener;
        listener.bind("0.0.0.0", port);

        cout_locked() << "Waiting for clients to register on " << port << "\n";

        while (true) {
            std::string client;
            int port;
            std::string data;
            std::tie(client, port, data) = listener.recv();
            if (data == "unregister") {
                if (registrations->removeClient(client, port)) {
                    cout_locked() << "Unregistered " << client << ":" << port << "\n";
                } else {
                    cerr_locked() << "Client " << client << ":" << port << " not registered or already unregistered\n";
                }
            } else if (data == "register") {
                if (registrations->addClient(client, port)) {
                    cout_locked() << "Registered client " << client << ":" << port << ".  Payload (" << data << ")\n";
                } else {
                    cerr_locked() << "Client " << client << ":" << port << " already registered\n";
                }
            } else {
                cerr_locked() << "Unsupported command (" << data << ") from client " << client << ":" << port << "\n";
            }
        }
    } catch (const std::exception& e) {
        cerr_locked() << "Unhandled exception in admin processor: " << e.what() << std::endl;
    }
}

static void udp_mirror(int incoming_port, ClientSet *registrations) {
    try {
        Udp listener;
        Udp mirror;

        listener.bind("0.0.0.0", incoming_port);

        cout_locked() << "Listening for incoming UDP data on " << incoming_port << "\n";

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

            // cout_locked() << "Re-broadcast datagram " << data.size() << " bytes to " << registered.size() << " clients\n";
        }
    } catch (const std::exception& e) {
        cerr_locked() << "Unhandled exception in receiver: " << e.what() << std::endl;
    }
}

int main(int argc, char **argv) {
    try {
        int incoming_port = 514;
        int outflow_admin_port = 10000;
        ClientSet registrations;
        
        //cout_locked() << "Starting mirror thread" << std::endl;
        std::thread mirror_thread(udp_mirror, incoming_port, &registrations);

        //cout_locked() << "Detaching mirror thread" << std::endl;
        mirror_thread.detach();

        //cout_locked() << "Starting admin thread" << std::endl;
        std::thread admin_thread(outflow_admin, outflow_admin_port, &registrations);

        //cout_locked() << "Waiting for admin thread to finish" << std::endl;
        admin_thread.join();
    } catch (const std::exception& e) {
        cerr_locked() << "Unhandled exception in main: " << e.what() << std::endl;
    }
}

