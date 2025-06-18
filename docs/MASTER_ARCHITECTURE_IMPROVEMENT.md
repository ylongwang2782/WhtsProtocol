# Master Server Architecture Improvement

## Overview

The Master server has been completely refactored to address the following issues:
1. **Missing Response Generation**: Original code only had TODO comments
2. **Poor Extensibility**: Hard to add new message types
3. **No Action Management**: No separation between response generation and action execution
4. **Lack of Device Management**: No tracking of connected slaves

## New Architecture

### 1. Message Handler Interface (`IMessageHandler`)

```cpp
class IMessageHandler {
public:
    virtual ~IMessageHandler() = default;
    virtual std::unique_ptr<Message> processMessage(const Message& message, MasterServer* server) = 0;
    virtual void executeActions(const Message& message, MasterServer* server) = 0;
};
```

**Benefits:**
- **Extensible**: Easy to add new message types by implementing the interface
- **Separation of Concerns**: Response generation and action execution are separate
- **Testable**: Each handler can be unit tested independently

### 2. Device Management (`DeviceManager`)

```cpp
class DeviceManager {
private:
    std::unordered_map<uint32_t, bool> connectedSlaves;
    std::unordered_map<uint32_t, uint8_t> slaveShortIds;
    
public:
    void addSlave(uint32_t slaveId, uint8_t shortId = 0);
    void removeSlave(uint32_t slaveId);
    bool isSlaveConnected(uint32_t slaveId) const;
    std::vector<uint32_t> getConnectedSlaves() const;
    uint8_t getSlaveShortId(uint32_t slaveId) const;
};
```

**Features:**
- **Slave Tracking**: Maintains list of connected slaves
- **Short ID Management**: Maps slave IDs to short IDs
- **Connection Status**: Tracks online/offline status

### 3. Concrete Message Handlers

#### SlaveConfigHandler
- **Response**: Creates `Master2Backend::SlaveConfigResponseMessage`
- **Actions**: Registers slaves in device manager, prepares for slave forwarding

#### ModeConfigHandler
- **Response**: Creates `Master2Backend::ModeConfigResponseMessage`
- **Actions**: Applies mode configuration to system

#### ResetHandler
- **Response**: Creates `Master2Backend::RstResponseMessage`
- **Actions**: Sends reset commands to specified slaves

#### ControlHandler
- **Response**: Creates `Master2Backend::CtrlResponseMessage`
- **Actions**: Applies control commands to system

#### PingControlHandler
- **Response**: Creates `Master2Backend::PingResponseMessage`
- **Actions**: Executes ping commands to target slaves

#### DeviceListHandler
- **Response**: Creates `Master2Backend::DeviceListResponseMessage` with real device data
- **Actions**: No additional actions needed

### 4. Enhanced MasterServer Class

```cpp
class MasterServer {
private:
    DeviceManager deviceManager;
    std::unordered_map<uint8_t, std::unique_ptr<IMessageHandler>> messageHandlers;
    
public:
    // Message sending methods
    void sendResponseToBackend(std::unique_ptr<Message> response, const sockaddr_in &clientAddr);
    void sendCommandToSlave(uint32_t slaveId, std::unique_ptr<Message> command, const sockaddr_in &clientAddr);
    
    // Device management access
    DeviceManager& getDeviceManager();
    
    // Handler registration
    void registerMessageHandler(uint8_t messageId, std::unique_ptr<IMessageHandler> handler);
};
```

## Key Improvements

### 1. Complete Response Generation
- All Backend2Master messages now generate proper responses
- Responses are based on the original main.cpp implementation
- Status codes and data are properly set

### 2. Action Management
- **Two-Phase Processing**: Response generation + Action execution
- **Flexible Actions**: Each handler can define custom actions
- **Future-Ready**: Easy to add complex actions like slave forwarding

### 3. Better Maintainability
- **Single Responsibility**: Each handler manages one message type
- **Easy Extension**: Add new handlers without modifying existing code
- **Clear Structure**: Separation between networking, processing, and business logic

### 4. Device Tracking
- **Real Device Lists**: Device list responses use actual connected slaves
- **Connection Management**: Tracks slave connections and disconnections
- **Short ID Support**: Manages slave short ID assignments

## Usage Examples

### Adding a New Message Type

```cpp
class NewMessageHandler : public IMessageHandler {
public:
    std::unique_ptr<Message> processMessage(const Message& message, MasterServer* server) override {
        // Generate response
        auto response = std::make_unique<SomeResponseMessage>();
        response->status = 0;
        return std::move(response);
    }
    
    void executeActions(const Message& message, MasterServer* server) override {
        // Execute actions
        Log::i("NewMessageHandler", "Executing custom actions");
    }
};

// Register the handler
server.registerMessageHandler(NEW_MESSAGE_ID, std::make_unique<NewMessageHandler>());
```

### Checking Slave Connection

```cpp
if (server->getDeviceManager().isSlaveConnected(slaveId)) {
    // Send command to slave
    auto command = std::make_unique<SomeSlaveCommand>();
    server->sendCommandToSlave(slaveId, std::move(command), clientAddr);
}
```

## Future Enhancements

### 1. Slave Command Forwarding
- Add methods to forward configuration commands to slaves
- Implement response aggregation from multiple slaves
- Add timeout handling for slave responses

### 2. Advanced Device Management
- Add device capabilities tracking
- Implement device health monitoring
- Add device firmware version management

### 3. Error Handling
- Add comprehensive error handling in handlers
- Implement retry mechanisms for failed operations
- Add error reporting to backend

### 4. Configuration Management
- Add configuration persistence
- Implement configuration validation
- Add configuration rollback capabilities

## Migration Guide

### From Original Code
1. **Response Generation**: All TODO comments have been replaced with actual implementations
2. **Message Processing**: Switch-case statements replaced with handler pattern
3. **Device Management**: Manual tracking replaced with DeviceManager
4. **Extensibility**: New message types can be added without modifying core code

### Testing Strategy
1. **Unit Tests**: Test each handler independently
2. **Integration Tests**: Test handler registration and message routing
3. **Device Management Tests**: Test slave connection tracking
4. **End-to-End Tests**: Test complete message flow from backend to slaves

This architecture provides a solid foundation for the Master server that is maintainable, extensible, and production-ready. 