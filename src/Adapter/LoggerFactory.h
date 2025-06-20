#pragma once
#include "../interface/ILogger.h"
#include <memory>

/**
 * @brief 日志工厂类
 * 根据编译时的平台选择不同的日志实现
 */
class LoggerFactory {
  public:
    /**
     * @brief 创建适合当前平台的日志实现
     * @return 日志实现的智能指针
     */
    static std::unique_ptr<ILogger> createLogger();

    /**
     * @brief 获取单例日志实例
     * @return 日志实例的引用
     */
    static ILogger &getInstance();

  private:
    static std::unique_ptr<ILogger> s_instance;
};