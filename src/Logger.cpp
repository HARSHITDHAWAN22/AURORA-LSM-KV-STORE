#include "Logger.h"
#include <iostream>
#include <chrono>
#include <ctime>

Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

Logger::~Logger() {
    if (outFile.is_open()) {
        outFile.close();
    }
}

void Logger::init(const std::string& filename, LogLevel level) {
    std::lock_guard<std::mutex> lock(mtx);
    outFile.open(filename, std::ios::app);
    currentLevel = level;
}

void Logger::setLevel(LogLevel level){
    std::lock_guard<std::mutex> lock(mtx);
    currentLevel = level;
}

void Logger::debug(const std::string& msg){
    log(LogLevel::DEBUG, msg);
}

void Logger::info(const std::string& msg){
    log(LogLevel::INFO, msg);
}

void Logger::error(const std::string& msg){
    log(LogLevel::ERROR, msg);
}

void Logger::log(LogLevel level, const std::string& msg){
    if (level < currentLevel) return;

    std::lock_guard<std::mutex> lock(mtx);

    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);

    if (outFile.is_open()){
        outFile << std::ctime(&now_c)
                << " [" << levelToString(level) << "] "
                << msg << "\n";
    }
}

std::string Logger::levelToString(LogLevel level){
    switch(level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO";
        case LogLevel::ERROR: return "ERROR";
        default: return "";
    }
}  