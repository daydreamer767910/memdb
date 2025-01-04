#include <iostream>
#include <string>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <queue>
#include <chrono>
#include "util/util.hpp"

// 环形缓冲区
class CircularBuffer {
public:
    explicit CircularBuffer(size_t size)
        : buffer_(new char[size]), capacity_(size), read_pos_(0), write_pos_(0), full_(false) {}

    ~CircularBuffer() {
        delete[] buffer_;
    }

    void clear() {
        std::unique_lock<std::mutex> lock(mutex_);
        read_pos_ = 0;
        write_pos_ = 0;
        full_ = false;

        // 通知所有可能等待写入或读取的线程
        write_cv_.notify_all();
        read_cv_.notify_all();
        std::cout << "Buffer cleared.\n";
    }

    // 写入数据到缓冲区，支持超时
    bool write(const char* data, size_t size, std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(mutex_);
        print_packet(reinterpret_cast<const uint8_t*>(data),size);
        if (!write_cv_.wait_for(lock, timeout, [this, size]() { return size <= availableSpace(); })) {
            std::cout << "timeout for write\n";
            return false;  // 超时未满足可写条件
        }

        size_t end_space = capacity_ - write_pos_;
        if (size <= end_space) {
            std::memcpy(buffer_ + write_pos_, data, size);
        } else {
            std::memcpy(buffer_ + write_pos_, data, end_space);
            std::memcpy(buffer_, data + end_space, size - end_space);
        }

        write_pos_ = (write_pos_ + size) % capacity_;
        full_ = (write_pos_ == read_pos_);
        read_cv_.notify_one();  // 通知可能有等待读取的线程
        return true;
    }

    // 从缓冲区读取数据，支持超时
    bool read(char* data, size_t size, std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(mutex_);

        if (!read_cv_.wait_for(lock, timeout, [this, size]() { return size <= readableSize(); })) {
            return false;  // 超时未满足可读条件
        }

        size_t end_space = capacity_ - read_pos_;
        if (size <= end_space) {
            std::memcpy(data, buffer_ + read_pos_, size);
        } else {
            std::memcpy(data, buffer_ + read_pos_, end_space);
            std::memcpy(data + end_space, buffer_, size - end_space);
        }

        read_pos_ = (read_pos_ + size) % capacity_;
        full_ = false;
        write_cv_.notify_one();  // 通知可能有等待写入的线程
        return true;
    }
private:
    // 获取可读数据大小
    size_t readableSize() const {
        //std::unique_lock<std::mutex> lock(mutex_);
        if (full_) {
            return capacity_;
        } else if (write_pos_ >= read_pos_) {
            return write_pos_ - read_pos_;
        } else {
            return capacity_ - read_pos_ + write_pos_;
        }
    }

    // 获取可用空间大小
    size_t availableSpace() const {
        return capacity_ - readableSize();
    }

private:
    char* buffer_;
    size_t capacity_;
    size_t read_pos_;
    size_t write_pos_;
    bool full_;
    mutable std::mutex mutex_;
    std::condition_variable read_cv_;
    std::condition_variable write_cv_;
};

