# TODO List

## 🚧 功能开发

- [x] 为从机上电新增一个设备ID配置，传入u64的deviceID，并且进行格式检查，如果不输入则使用默认值
- [ ] 完成从机的数据采集功能。
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
当主机被MODE_CFG_MSG配置为启动模式后，需要开始不断从从机获得数据
1. 发送Sync Message，这将让所有从机开始采集数据
2. 等待所有从机数据采集完成后，根据当前模式发送Read conduction data Message从从机读取相应数据
3. 接收到从机的CONDUCTION_DATA_MSG后，将其转发给后端

## 🧹 技术债务

## 🐞 Bug 修复

## 📖 文档相关
