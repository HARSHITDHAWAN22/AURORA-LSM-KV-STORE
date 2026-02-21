#pragma once
#include <string>
#include <mutex>
#include <fstream>
#include <memory>

enum class LogLevel{
    DEBUG = 0,
    INFO,
    ERROR,
    NONE
};

class Logger{
public:
    static Logger& getInstance();

    void init(const std::string& filename, LogLevel level);
    void setLevel(LogLevel level);

    void debug(const std::string& msg);
    void info(const std::string& msg);
    void error(const std::string& msg);


private:
    Logger() = default;
    ~Logger();

    void log(LogLevel level, const std::string& msg);
    std::string levelToString(LogLevel level);

    std::mutex mtx;
    std::ofstream outFile;
    LogLevel currentLevel = LogLevel::INFO;
};

#define LOG_DEBUG(msg) Logger::getInstance().debug(msg)
#define LOG_INFO(msg)  Logger::getInstance().info(msg)
#define LOG_ERROR(msg) Logger::getInstance().error(msg)