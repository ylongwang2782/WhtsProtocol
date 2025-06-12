#include "WhtsProtocol.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <sstream>
#include <chrono>

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
    
    // 将十六进制字符串转换为字节数组
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
    
    // 将字节数组转换为十六进制字符串
    std::string bytesToHexString(const std::vector<uint8_t>& bytes) {
        std::stringstream ss;
        for (uint8_t byte : bytes) {
            ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
        }
        return ss.str();
    }
    
    // 打印字节数组
    void printBytes(const std::vector<uint8_t>& data, const std::string& description) {
        std::cout << description << " (" << data.size() << " bytes): ";
        for (auto byte : data) {
            std::cout << std::hex << std::setw(2) << std::setfill('0') 
                      << static_cast<int>(byte) << " ";
        }
        std::cout << std::dec << std::endl;
    }
    
    // 获取当前时间戳
    uint32_t getCurrentTimestamp() {
        return static_cast<uint32_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()
            ).count()
        );
    }
    
    // 处理Master2Slave消息并生成响应
    std::unique_ptr<Message> processAndCreateResponse(uint32_t slaveId, const Message& request) {
        switch (request.getMessageId()) {
            case static_cast<uint8_t>(Master2SlaveMessageId::SYNC_MSG): {
                auto syncMsg = dynamic_cast<const Master2Slave::SyncMessage*>(&request);
                if (syncMsg) {
                    std::cout << "处理同步消息 - 模式: " << static_cast<int>(syncMsg->mode) 
                              << ", 时间戳: " << syncMsg->timestamp << std::endl;
                    // 同步消息通常不需要响应，但我们可以发送一个状态消息
                    return nullptr;
                }
                break;
            }
            
            case static_cast<uint8_t>(Master2SlaveMessageId::CONDUCTION_CFG_MSG): {
                auto configMsg = dynamic_cast<const Master2Slave::ConductionConfigMessage*>(&request);
                if (configMsg) {
                    std::cout << "处理导通配置 - 时隙: " << static_cast<int>(configMsg->timeSlot)
                              << ", 间隔: " << static_cast<int>(configMsg->interval) << "ms" << std::endl;
                    
                    auto response = std::make_unique<Slave2Master::ConductionConfigResponseMessage>();
                    response->status = 0;  // 成功
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
                    std::cout << "处理阻值配置 - 时隙: " << static_cast<int>(configMsg->timeSlot)
                              << ", 间隔: " << static_cast<int>(configMsg->interval) << "ms" << std::endl;
                    
                    auto response = std::make_unique<Slave2Master::ResistanceConfigResponseMessage>();
                    response->status = 0;  // 成功
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
                    std::cout << "处理卡钉配置 - 间隔: " << static_cast<int>(configMsg->interval)
                              << "ms, 模式: " << static_cast<int>(configMsg->mode) << std::endl;
                    
                    auto response = std::make_unique<Slave2Master::ClipConfigResponseMessage>();
                    response->status = 0;  // 成功
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
                    std::cout << "处理Ping请求 - 序列号: " << pingMsg->sequenceNumber 
                              << ", 时间戳: " << pingMsg->timestamp << std::endl;
                    
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
                    std::cout << "处理复位消息 - 锁状态: " << static_cast<int>(rstMsg->lockStatus) << std::endl;
                    
                    auto response = std::make_unique<Slave2Master::RstResponseMessage>();
                    response->status = 0;  // 成功
                    response->lockStatus = rstMsg->lockStatus;
                    response->clipLed = rstMsg->clipLed;
                    return std::move(response);
                }
                break;
            }
            
            case static_cast<uint8_t>(Master2SlaveMessageId::SHORT_ID_ASSIGN_MSG): {
                auto assignMsg = dynamic_cast<const Master2Slave::ShortIdAssignMessage*>(&request);
                if (assignMsg) {
                    std::cout << "处理短ID分配 - 短ID: " << static_cast<int>(assignMsg->shortId) << std::endl;
                    
                    auto response = std::make_unique<Slave2Master::ShortIdConfirmMessage>();
                    response->status = 0;  // 成功
                    response->shortId = assignMsg->shortId;
                    return std::move(response);
                }
                break;
            }
            
            default:
                std::cout << "未知消息类型: 0x" << std::hex << static_cast<int>(request.getMessageId()) << std::dec << std::endl;
                break;
        }
        
        return nullptr;
    }
    
    // 处理接收到的数据
    void processReceivedData(const std::vector<uint8_t>& data, const sockaddr_in& clientAddr) {
        std::cout << "\n=== 收到数据 ===" << std::endl;
        printBytes(data, "接收到的原始数据");
        
        // 解析帧
        Frame frame;
        if (!processor.parseFrame(data, frame)) {
            std::cout << "❌ 帧解析失败" << std::endl;
            return;
        }
        
        std::cout << "✅ 帧解析成功:" << std::endl;
        std::cout << "  包类型: 0x" << std::hex << static_cast<int>(frame.packetId) << std::dec << std::endl;
        std::cout << "  分片序列: " << static_cast<int>(frame.fragmentsSequence) << std::endl;
        std::cout << "  更多分片: " << static_cast<int>(frame.moreFragmentsFlag) << std::endl;
        std::cout << "  载荷长度: " << frame.packetLength << std::endl;
        
        // 根据包类型处理
        if (frame.packetId == static_cast<uint8_t>(PacketId::MASTER_TO_SLAVE)) {
            uint32_t destinationId;
            std::unique_ptr<Message> message;
            
            if (processor.parseMaster2SlavePacket(frame.payload, destinationId, message)) {
                std::cout << "✅ Master2Slave包解析成功:" << std::endl;
                std::cout << "  目标ID: 0x" << std::hex << destinationId << std::dec << std::endl;
                std::cout << "  消息ID: 0x" << std::hex << static_cast<int>(message->getMessageId()) << std::dec << std::endl;
                
                // 模拟从机ID
                uint32_t slaveId = 0x12345678;
                
                // 处理消息并生成响应
                auto response = processAndCreateResponse(slaveId, *message);
                
                if (response) {
                    // 打包响应消息
                    auto responseData = processor.packSlave2MasterMessage(slaveId, *response);
                    
                    std::cout << "📤 发送响应:" << std::endl;
                    printBytes(responseData, "响应数据");
                    
                    // 发送响应
                    sendto(sock, reinterpret_cast<const char*>(responseData.data()), 
                           static_cast<int>(responseData.size()), 0, 
                           (sockaddr*)&clientAddr, sizeof(clientAddr));
                    
                    std::cout << "✅ 响应已发送" << std::endl;
                } else {
                    std::cout << "ℹ️ 无需响应此消息" << std::endl;
                }
            } else {
                std::cout << "❌ Master2Slave包解析失败" << std::endl;
            }
        } else {
            std::cout << "⚠️ 不支持的包类型: 0x" << std::hex << static_cast<int>(frame.packetId) << std::dec << std::endl;
        }
    }
    
    // 主循环
    void run() {
        std::cout << "🚀 协议测试器已启动，等待UDP消息..." << std::endl;
        std::cout << "💡 提示：发送十六进制数据到 localhost:" << port << std::endl;
        std::cout << "📋 示例数据 (Sync消息): ab cd 00 00 00 0a 00 00 21 43 65 87 01 78 56 34 12" << std::endl;
        std::cout << "按 Ctrl+C 退出\n" << std::endl;
        
        char buffer[1024];
        sockaddr_in clientAddr;
        socklen_t clientAddrLen = sizeof(clientAddr);
        
        while (true) {
            int bytesReceived = recvfrom(sock, buffer, sizeof(buffer), 0, 
                                       (sockaddr*)&clientAddr, &clientAddrLen);
            
            if (bytesReceived > 0) {
                // 检查是否是十六进制字符串格式
                std::string receivedStr(buffer, bytesReceived);
                
                // 移除空格和换行符
                receivedStr.erase(std::remove_if(receivedStr.begin(), receivedStr.end(), 
                    [](char c) { return std::isspace(c); }), receivedStr.end());
                
                std::vector<uint8_t> data;
                
                // 如果是十六进制字符串，转换为字节
                if (receivedStr.length() > 0 && std::all_of(receivedStr.begin(), receivedStr.end(), 
                    [](char c) { return std::isxdigit(c); })) {
                    data = hexStringToBytes(receivedStr);
                    std::cout << "📥 接收到十六进制字符串: " << receivedStr << std::endl;
                } else {
                    // 否则直接作为二进制数据处理
                    data.assign(buffer, buffer + bytesReceived);
                    std::cout << "📥 接收到二进制数据" << std::endl;
                }
                
                if (!data.empty()) {
                    processReceivedData(data, clientAddr);
                }
            }
        }
    }
};

int main() {
    std::cout << "WhtsProtocol UDP 测试器" << std::endl;
    std::cout << "======================" << std::endl;
    
    try {
        UdpProtocolTester tester(8888);
        tester.run();
    } catch (const std::exception& e) {
        std::cerr << "错误: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
} 