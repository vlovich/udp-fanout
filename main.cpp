#include <thread>

#include <libgen.h>

#include "net.h"
#include "locked_ostream.h"
#include "client_set.h"

#include <cstring>
#include <signal.h>
#include <atomic>

static std::mutex console_lock;

#define cout_locked() LockedOStream(std::cout, console_lock)
#define cerr_locked() LockedOStream(std::cerr, console_lock)

class SignalBlocker {
public:
    SignalBlocker() {
        sigemptyset(&signal_mask);
    }

    SignalBlocker(std::initializer_list<int> signals) 
        : SignalBlocker()
    {
        for (auto signal : signals) {
            addSignal(signal);
        }
    }

    ~SignalBlocker() {
        unblock();
    }
    
    void addSignal(int signal) {
        sigaddset(&signal_mask, signal);
    }

    void block() {
        int rc = pthread_sigmask(SIG_BLOCK, &signal_mask, nullptr);
        if (rc != 0) {
            throw std::runtime_error("Couldn't block signal: " + std::string(std::strerror(rc)));
        }
    }

    void unblock() {
        int rc = pthread_sigmask(SIG_UNBLOCK, &signal_mask, nullptr);
        if (rc != 0) {
            throw std::runtime_error("Couldn't block signal: " + std::string(std::strerror(rc)));
        }
    }

private:
    sigset_t signal_mask;
};

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

	cout_locked() << "Admin thread finished" << std::endl;
    } catch (const std::exception& e) {
        cerr_locked() << "Unhandled exception in admin processor: " << e.what() << std::endl;
        exit(2);
    }
}

static void udp_mirror(int incoming_port, ClientSet *registrations) {
    try {
        Udp listener;

        listener.bind("0.0.0.0", incoming_port);

        cout_locked() << "Listening for incoming UDP data on " << incoming_port << "\n";

        while (true) {
            std::string sender;
            int sender_port;
            std::string data;
            std::tie(sender, sender_port, data) = listener.recv();

            auto registered = registrations->getClientList();

            for (const auto& client : registered) {
                try {
                    // TODO: spoof src IP/PORT to be sender/sender_port
                    listener.send(std::get<0>(client), std::get<1>(client), data);
                } catch (const std::exception& e) {
                    cerr_locked() << "Couldn't send re-broadcast packet to " << std::get<0>(client) << ":" << std::get<1>(client) << "\n";
                }
            }

            // cout_locked() << "Re-broadcast datagram " << data.size() << " bytes to " << registered.size() << " clients\n";
        }

	cout_locked() << "UDP mirror thread finished" << std::endl;
    } catch (const std::exception& e) {
        cerr_locked() << "Unhandled exception in receiver: " << e.what() << std::endl;
        exit(2);
    }
}

int main(int argc, char **argv) {
    try {
        int nargs = argc - 1;
        if (nargs != 2) {
            cerr_locked() << "Expecting 2 arguments, but got " << nargs << "\n"
                          << "Usage: " << basename(argv[0]) << " <udp port> <admin port>" << "\n"
                          << "    udp port         The port to listen for UDP packets on.  These are then\n"
                          << "                     re-transmitted based to the registered listeners\n"
                          << "    admin port       The port to listen on for registration/deregistration commands.\n"
                          ;
            return 1;
        }
        int incoming_port = atoi(argv[1]);
        int outflow_admin_port = atoi(argv[2]);
        if (incoming_port == 0 || outflow_admin_port == 0) {
            if (incoming_port == 0) {
                cerr_locked() << "Invalid UDP fanout listen port '" << argv[1] << "'\n";
            }
            if (outflow_admin_port == 0) {
                cerr_locked() << "Invalid admin port '" << argv[2] << "'\n";
            }
            return 1;
        }

        ClientSet registrations;

        // mask out the SIGINT signal so that when we create the child threads, we know that
        // there will not be signals delivered to them.
        SignalBlocker signalBlocker = {SIGINT};
        signalBlocker.block();
        
        //cout_locked() << "Starting mirror thread" << std::endl;
        std::thread mirror_thread(udp_mirror, incoming_port, &registrations);

        //cout_locked() << "Starting admin thread" << std::endl;
        std::thread admin_thread(outflow_admin, outflow_admin_port, &registrations);
        
        // child threads are created (inherting the block of the required signals), we can re-enable signals
        // and know that only the main thread will process signals
        signalBlocker.unblock();
        
        //cout_locked() << "Detaching mirror thread" << std::endl;
        mirror_thread.detach();

        //cout_locked() << "Waiting for admin thread to finish" << std::endl;
        admin_thread.join();

        return 0;
    } catch (const std::exception& e) {
        cerr_locked() << "Unhandled exception in main: " << e.what() << std::endl;
    } catch (...) {
        cerr_locked() << "Unknown exception in main";
    }

    return 2;
}

