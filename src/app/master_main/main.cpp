#include "Logger.h"
#include "MasterServer.h"

int main() {
    Log::i("Main", "WhtsProtocol Master Server");
    Log::i("Main", "==========================");

    // 测试格式化日志功能
    int backendPort = 8079;
    int masterPort = 8080;
    int slavePort = 8081;

    Log::i("Main", "Port Configuration (Wireless Broadcast Simulation):");
    Log::i("Main", "  Backend: %d (receives responses from Master)",
           backendPort);
    Log::i("Main", "  Master:  %d (listens for Backend & Slave messages)",
           masterPort);
    Log::i("Main", "  Slaves:  %d (receive broadcast commands from Master)",
           slavePort);
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
