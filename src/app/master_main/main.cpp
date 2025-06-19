#include "Logger.h"
#include "MasterServer.h"

int main() {
    Log::i("Main", "WhtsProtocol Master Server");
    Log::i("Main", "==========================");
    Log::i("Main", "Port Configuration (Wireless Broadcast Simulation):");
    Log::i("Main", "  Backend: 8079 (receives responses from Master)");
    Log::i("Main", "  Master:  8080 (listens for Backend & Slave messages)");
    Log::i("Main", "  Slaves:  8081 (receive broadcast commands from Master)");
    Log::i("Main", "Wireless Communication Simulation:");
    Log::i("Main",
           "  Master -> Slaves: UDP Broadcast (simulates wireless broadcast)");
    Log::i("Main",
           "  Slaves -> Master: UDP Unicast (simulates wireless response)");
    Log::i("Main", "Handling Backend2Master and Slave2Master packets");

    try {
        MasterServer server(8080);
        server.run();
    } catch (const std::exception &e) {
        Log::e("Main", "Error: %s", e.what());
        return 1;
    }

    return 0;
}
