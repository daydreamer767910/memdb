#ifndef BI_CHANNEL_HPP
#define BI_CHANNEL_HPP

#include <vector>
#include <mutex>
#include <condition_variable>
#include <stdexcept>
#include <cstring> // for memcpy
#include <cstdint>

class CircularBuffer {
public:
    explicit CircularBuffer(size_t capacity)
        : buffer_(capacity), read_index_(0), write_index_(0), size_(0) {}

    int32_t write(const uint8_t* data, size_t len) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (size_ + len > buffer_.size()) {
            return -1; // 缓冲区空间不足
        }
        for (size_t i = 0; i < len; ++i) {
            buffer_[write_index_] = data[i];
            write_index_ = (write_index_ + 1) % buffer_.size();
        }
        size_ += len;
        cond_empty_.notify_all();
        return len;
    }

    int32_t read(uint8_t* data, size_t len) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (size_ < len) {
            return -1; // 缓冲区数据不足
        }
        for (size_t i = 0; i < len; ++i) {
            data[i] = buffer_[read_index_];
            read_index_ = (read_index_ + 1) % buffer_.size();
        }
        size_ -= len;
        cond_full_.notify_all();
        return len;
    }

    int32_t peek(uint8_t* data, size_t len) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (size_ < len) {
            return -1; // 缓冲区数据不足
        }
        for (size_t i = 0; i < len; ++i) {
            data[i] = buffer_[(read_index_ + i) % buffer_.size()];
        }
        return len;
    }

    bool is_empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return size_ == 0;
    }

    bool is_full() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return size_ == buffer_.size();
    }

    size_t available_space() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return buffer_.size() - size_;
    }

    size_t available_data() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return size_;
    }

private:
    std::vector<uint8_t> buffer_;
    size_t read_index_;
    size_t write_index_;
    size_t size_;
    mutable std::mutex mutex_;
    std::condition_variable cond_full_;
    std::condition_variable cond_empty_;
};

class DoubleChannel {
public:
    explicit DoubleChannel(size_t capacity)
        : up_channel_(capacity), down_channel_(capacity) {}

    int32_t write_up(const uint8_t* data, size_t len) {
        return up_channel_.write(data, len);
    }

    int32_t read_up(uint8_t* data, size_t len) {
        return up_channel_.read(data, len);
    }

    int32_t write_down(const uint8_t* data, size_t len) {
        return down_channel_.write(data, len);
    }

    int32_t read_down(uint8_t* data, size_t len) {
        return down_channel_.read(data, len);
    }

    int32_t read_message_length_up() {
        uint8_t length_data[4];
        if (up_channel_.peek(length_data, 4)) {
            return (length_data[0] << 24) | (length_data[1] << 16) | (length_data[2] << 8) | length_data[3];
        }
        return -1; // 无法读取长度
    }

    int32_t read_message_length_down() {
        uint8_t length_data[4];
        if (down_channel_.peek(length_data, 4)) {
            return (length_data[0] << 24) | (length_data[1] << 16) | (length_data[2] << 8) | length_data[3];
        }
        return -1; // 无法读取长度
    }

private:
    CircularBuffer up_channel_;
    CircularBuffer down_channel_;
};



#endif