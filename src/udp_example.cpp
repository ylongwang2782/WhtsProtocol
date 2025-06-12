#include "WhtsProtocol.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <sstream>
#include <chrono>
#include <algorithm>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef int socklen_t;
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #define SOCKET int
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
    #define closesocket close
#endif

using namespace WhtsProtocol;

class UdpProtocolTester {
private:
    SOCKET sock;
    sockaddr_in serverAddr;
    ProtocolProcessor processor;
    uint16_t port;
    
public:
    UdpProtocolTester(uint16_t listenPort = 8888) : port(listenPort) {
#ifdef _WIN32
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            throw std::runtime_error("WSAStartup failed");
        }
#endif
        
        sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock == INVALID_SOCKET) {
            throw std::runtime_error("Socket creation failed");
        }
        
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = INADDR_ANY;
        serverAddr.sin_port = htons(port);
        
        if (bind(sock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            closesocket(sock);
            throw std::runtime_error("Bind failed");
        }
        
        std::cout << "UDP Protocol Tester listening on port " << port << std::endl;
    }
    
    ~UdpProtocolTester() {
        closesocket(sock);
#ifdef _WIN32
        WSACleanup();
#endif
    }
    
    // Convert a hexadecimal string to a byte array
    std::vector<uint8_t> hexStringToBytes(const std::string& hex) {
        std::vector<uint8_t> bytes;
        for (size_t i = 0; i < hex.length(); i += 2) {
            if (i + 1 < hex.length()) {
                std::string byteString = hex.substr(i, 2);
                uint8_t byte = static_cast<uint8_t>(strtol(byteString.c_str(), nullptr, 16));
                bytes.push_back(byte);
            }
        }
        return bytes;
    }
    
    // Convert a byte array to a hexadecimal string
    std::string bytesToHexString(const std::vector<uint8_t>& bytes) {
        std::stringstream ss;
        for (uint8_t byte : bytes) {
            ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
        }
        return ss.str();
    }
    
    // Print the byte array
    void printBytes(const std::vector<uint8_t>& data, const std::string& description) {
        std::cout << description << " (" << data.size() << " bytes): ";
        for (auto byte : data) {
            std::cout << std::hex << std::setw(2) << std::setfill('0') 
                      << static_cast<int>(byte) << " ";
        }
        std::cout << std::dec << std::endl;
    }
    
    // Get the current timestamp
    uint32_t getCurrentTimestamp() {
        return static_cast<uint32_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()
            ).count()
        );
    }
    
    // Process Master2Slave messages and generate responses
    std::unique_ptr<Message> processAndCreateResponse(uint32_t slaveId, const Message& request) {
        switch (request.getMessageId()) {
            case static_cast<uint8_t>(Master2SlaveMessageId::SYNC_MSG): {
                auto syncMsg = dynamic_cast<const Master2Slave::SyncMessage*>(&request);
                if (syncMsg) {
                    std::cout << "Processing sync message - Mode: " << static_cast<int>(syncMsg->mode) 
                              << ", Timestamp: " << syncMsg->timestamp << std::endl;
                    // Sync messages usually don't require a response, but we can send a status message
                    return nullptr;
                }
                break;
            }
            
            case static_cast<uint8_t>(Master2SlaveMessageId::CONDUCTION_CFG_MSG): {
                auto configMsg = dynamic_cast<const Master2Slave::ConductionConfigMessage*>(&request);
                if (configMsg) {
                    std::cout << "Processing conduction configuration - Time slot: " << static_cast<int>(configMsg->timeSlot)
                              << ", Interval: " << static_cast<int>(configMsg->interval) << "ms" << std::endl;
                    
                    auto response = std::make_unique<Slave2Master::ConductionConfigResponseMessage>();
                    response->status = 0;  // Success
                    response->timeSlot = configMsg->timeSlot;
                    response->interval = configMsg->interval;
                    response->totalConductionNum = configMsg->totalConductionNum;
                    response->startConductionNum = configMsg->startConductionNum;
                    response->conductionNum = configMsg->conductionNum;
                    return std::move(response);
                }
                break;
            }
            
            case static_cast<uint8_t>(Master2SlaveMessageId::RESISTANCE_CFG_MSG): {
                auto configMsg = dynamic_cast<const Master2Slave::ResistanceConfigMessage*>(&request);
                if (configMsg) {
                    std::cout << "Processing resistance configuration - Time slot: " << static_cast<int>(configMsg->timeSlot)
                              << ", Interval: " << static_cast<int>(configMsg->interval) << "ms" << std::endl;
                    
                    auto response = std::make_unique<Slave2Master::ResistanceConfigResponseMessage>();
                    response->status = 0;  // Success
                    response->timeSlot = configMsg->timeSlot;
                    response->interval = configMsg->interval;
                    response->totalConductionNum = configMsg->totalNum;
                    response->startConductionNum = configMsg->startNum;
                    response->conductionNum = configMsg->num;
                    return std::move(response);
                }
                break;
            }
            
            case static_cast<uint8_t>(Master2SlaveMessageId::CLIP_CFG_MSG): {
                auto configMsg = dynamic_cast<const Master2Slave::ClipConfigMessage*>(&request);
                if (configMsg) {
                    std::cout << "Processing clip configuration - Interval: " << static_cast<int>(configMsg->interval)
                              << "ms, Mode: " << static_cast<int>(configMsg->mode) << std::endl;
                    
                    auto response = std::make_unique<Slave2Master::ClipConfigResponseMessage>();
                    response->status = 0;  // Success
                    response->interval = configMsg->interval;
                    response->mode = configMsg->mode;
                    response->clipPin = configMsg->clipPin;
                    return std::move(response);
                }
                break;
            }
            
            case static_cast<uint8_t>(Master2SlaveMessageId::PING_REQ_MSG): {
                auto pingMsg = dynamic_cast<const Master2Slave::PingReqMessage*>(&request);
                if (pingMsg) {
                    std::cout << "Processing Ping request - Sequence number: " << pingMsg->sequenceNumber 
                              << ", Timestamp: " << pingMsg->timestamp << std::endl;
                    
                    auto response = std::make_unique<Slave2Master::PingRspMessage>();
                    response->sequenceNumber = pingMsg->sequenceNumber;
                    response->timestamp = getCurrentTimestamp();
                    return std::move(response);
                }
                break;
            }
            
            case static_cast<uint8_t>(Master2SlaveMessageId::RST_MSG): {
                auto rstMsg = dynamic_cast<const Master2Slave::RstMessage*>(&request);
                if (rstMsg) {
                    std::cout << "Processing reset message - Lock status: " << static_cast<int>(rstMsg->lockStatus) << std::endl;
                    
                    auto response = std::make_unique<Slave2Master::RstResponseMessage>();
                    response->status = 0;  // Success
                    response->lockStatus = rstMsg->lockStatus;
                    response->clipLed = rstMsg->clipLed;
                    return std::move(response);
                }
                break;
            }
            
            case static_cast<uint8_t>(Master2SlaveMessageId::SHORT_ID_ASSIGN_MSG): {
                auto assignMsg = dynamic_cast<const Master2Slave::ShortIdAssignMessage*>(&request);
                if (assignMsg) {
                    std::cout << "Processing short ID assignment - Short ID: " << static_cast<int>(assignMsg->shortId) << std::endl;
                    
                    auto response = std::make_unique<Slave2Master::ShortIdConfirmMessage>();
                    response->status = 0;  // Success
                    response->shortId = assignMsg->shortId;
                    return std::move(response);
                }
                break;
            }
            
            default:
                std::cout << "Unknown message type: 0x" << std::hex << static_cast<int>(request.getMessageId()) << std::dec << std::endl;
                break;
        }
        
        return nullptr;
    }
    
    // Process the received data
    void processReceivedData(const std::vector<uint8_t>& data, const sockaddr_in& clientAddr) {
        std::cout << "\n=== Data received ===" << std::endl;
        printBytes(data, "Received raw data");
        
        // 解析帧
        Frame frame;
        if (!processor.parseFrame(data, frame)) {
            std::cout << "Frame parsing failed" << std::endl;
            return;
        }
        
        std::cout << "Frame parsed successfully:" << std::endl;
        std::cout << "Packet type: 0x" << std::hex << static_cast<int>(frame.packetId) << std::dec << std::endl;
        std::cout << "Fragment sequence: " << static_cast<int>(frame.fragmentsSequence) << std::endl;
        std::cout << "More fragments: " << static_cast<int>(frame.moreFragmentsFlag) << std::endl;
        std::cout << "Payload length: " << frame.packetLength << std::endl;
        
        // 根据包类型处理
        if (frame.packetId == static_cast<uint8_t>(PacketId::MASTER_TO_SLAVE)) {
            uint32_t destinationId;
            std::unique_ptr<Message> message;
            
            if (processor.parseMaster2SlavePacket(frame.payload, destinationId, message)) {
                std::cout << "Master2Slave packet parsed successfully:" << std::endl;
                std::cout << "Destination ID: 0x" << std::hex << destinationId << std::dec << std::endl;
                std::cout << "Message ID: 0x" << std::hex << static_cast<int>(message->getMessageId()) << std::dec << std::endl;
                
                // 模拟从机ID
                uint32_t slaveId = 0x12345678;
                
                // 处理消息并生成响应
                auto response = processAndCreateResponse(slaveId, *message);
                
                if (response) {
                    // 打包响应消息
                    auto responseData = processor.packSlave2MasterMessage(slaveId, *response);
                    
                    std::cout << "Sending response:" << std::endl;
                    printBytes(responseData, "Response data");
                    
                    // 发送响应
                    sendto(sock, reinterpret_cast<const char*>(responseData.data()), 
                           static_cast<int>(responseData.size()), 0, 
                           (sockaddr*)&clientAddr, sizeof(clientAddr));
                    
                    std::cout << "Response sent" << std::endl;
                } else {
                    std::cout << "No response needed for this message" << std::endl;
                }
            } else {
                std::cout << "Failed to parse Master2Slave packet" << std::endl;
            }
        } else {
            std::cout << "Unsupported packet type: 0x" << std::hex << static_cast<int>(frame.packetId) << std::dec << std::endl;
        }
    }
    
    // Main loop
    void run() {
        std::cout << "Protocol tester started, waiting for UDP messages..." << std::endl;
        std::cout << "Tip: Send hexadecimal data to localhost:" << port << std::endl;
        std::cout << "Example data (Sync message): ab cd 00 00 00 0a 00 00 21 43 65 87 01 78 56 34 12" << std::endl;
        std::cout << "Press Ctrl+C to exit\n" << std::endl;
        
        char buffer[1024];
        sockaddr_in clientAddr;
        socklen_t clientAddrLen = sizeof(clientAddr);
        
        while (true) {
            int bytesReceived = recvfrom(sock, buffer, sizeof(buffer), 0, 
                                       (sockaddr*)&clientAddr, &clientAddrLen);
            
            if (bytesReceived > 0) {
                // Check if it's in hexadecimal string format
                std::string receivedStr(buffer, bytesReceived);
                
                // Remove spaces and line breaks
                receivedStr.erase(std::remove_if(receivedStr.begin(), receivedStr.end(), 
                    [](char c) { return std::isspace(c); }), receivedStr.end());
                
                std::vector<uint8_t> data;
                
                // If it's a hexadecimal string, convert it to bytes
                if (receivedStr.length() > 0 && std::all_of(receivedStr.begin(), receivedStr.end(), 
                    [](char c) { return std::isxdigit(c); })) {
                    data = hexStringToBytes(receivedStr);
                    std::cout << "Received hexadecimal string: " << receivedStr << std::endl;
                } else {
                    // Otherwise, treat it as binary data directly
                    data.assign(buffer, buffer + bytesReceived);
                    std::cout << "Received binary data" << std::endl;
                }
                
                if (!data.empty()) {
                    processReceivedData(data, clientAddr);
                }
            }
        }
    }
};

int main() {
    std::cout << "WhtsProtocol UDP tester" << std::endl;
    std::cout << "======================" << std::endl;
    
    try {
        UdpProtocolTester tester(8888);
        tester.run();
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}