# TODO List

## 🚧 功能开发

- [x] 移除slave私有的networkManager，像master一样使用公共网络管理器
- [x] 彻底删除HAL文件夹及其依赖文件
- [x] 虽然HAL库文件夹已经被移除了，但曾经在HAL文件夹下的IGpio和Network仍旧还在使用HAL命名空间，interface文件夹下的文件应该统一使用Interface命名空间更合理，并且Network类也应该更名为INetwork
- [x] 修复 master udp 通信异常的问题。master和slave使用的同一个接口库，但slave的udp收发正常，但master无法送udp接受到数据

- [x] 为了更好的分层次管理，将Gpio文件夹下的文件按功能粒度迁移，并保证slave_main能够正常编译
│      Gpio.cpp -> ./src/app/slave_main ✓
│      Gpio.h -> ./src/app/slave_main ✓
│      GpioFactory.cpp -> ./src/Adapter ✓
│      GpioFactory.h -> ./src/Adapter ✓
│      HardwareGpio.cpp -> ./src/platform/embedded ✓
│      HardwareGpio.h -> ./src/platform/embedded ✓
│      IGpio.h -> ./src/interface ✓
│      VirtualGpio.cpp -> ./src/platform/windows ✓
│      VirtualGpio.h -> ./src/platform/windows ✓

- [x] 确认一下master_main中使用的网络库
是使用的windows的网络库，还是使用的本工程下的NetworkHAL的接口库。
如果是使用的windows的网络库，请修改一下使用本工程下的NetworkHAL接口库，也就是使用./HAL/Network/NetworkManager.h

- [x] 为了更好的分层次管理，将Network文件夹下的文件按功能粒度迁移，并保证master_main能够正常编译
└─Network
        AsioUdpSocket.cpp -> ./src/platform/windows ✓
        AsioUdpSocket.h -> ./src/platform/windows ✓
        asio_example.cpp -> delete ✓
        CMakeLists.txt -> delete ✓
        example_usage.cpp -> delete ✓
        IUdpSocket.h -> ./src/interface ✓
        LwipUdpSocket.cpp -> ./src/platform/embedded ✓
        LwipUdpSocket.h -> ./src/platform/embedded ✓
        NetworkFactory.cpp -> ./src/Adapter ✓
        NetworkFactory.h -> ./src/Adapter ✓
        NetworkManager.cpp -> ./src/app ✓
        NetworkManager.h -> ./src/app ✓
        README_ASIO.md -> docs ✓
        WindowsUdpSocket.cpp -> ./src/platform/windows ✓
        WindowsUdpSocket.h -> ./src/platform/windows ✓

- [x] 确认一下slave_main中使用的网络库
使用的是windows的网络库，还是使用的本工程下./src/app/NetworkManager的接口库。
如果是使用的windows的网络库，请修改为使用本工程下/src/app/NetworkManager接口库

- [x] 从机程序启动后，新增输入Device ID然后再启动，默认Device ID 为0x00000001
- [x] 完成从机的数据采集功能。
1. 收到Conduction Config message后根据消息中的信息配置ContinuityCollector
```cpp
// 创建采集器
auto collector = Adapter::ContinuityCollectorFactory::
    createWithVirtualGpio();
// 配置采集
Adapter::CollectorConfig config(
    2, 0, 4, 20); // 2引脚, 从周期0开始, 总共4周期, 20ms间隔
collector->configure(config);
```

2. 收到Sync Message后开始采集，采集完成后进入待机状态，等待接受消息
```cpp
// 开始采集
collector->startCollection();
// 等待完成并获取数据
while (!collector->isCollectionComplete()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}
```

3. 接受到来自主机的Read Conduction Data Message，然后从Collector获得数据并创建response并回复
```cpp
response->conductionData = collector->getDataVector();
response->conductionLength = response->conductionData.size();
```

- [ ] 完善主机的数据采集逻辑。
当主机被MODE_CFG_MSG配置为启动模式后，开始不断从从机读取数据，整个流程描述如下
1. 主机发送Sync Message，这将让所有从机开始采集数据
2. 主机等待所有从机数据采集完成后，发送Read conduction data Message从从机读取相应数据
3. 主机接收到从机的CONDUCTION_DATA_MSG后，将原始数据不经过处理直接转发给后端

## 🧹 技术债务

- [x] Slave的工作流程优化
1. Slave main 和ContinuityCollector中用到了Thread，我认为没有必要使用thead，当前是一个逻辑非常简单的程序，优化程序，建议使用状态机来优化程序，这样更稳定，避免使用thread
2. 确保收到Sync Message后就立即执行数据采集以确保收到Read Conduction后能及时上传最新的数据

## 🐞 Bug 修复

- [x] 从机有严重bug，发送两次Sync Message就必死机
- [ ] 主机接受到Slave Config开始存储配置前没有清空上一次的配置

## 📖 文档相关
