#include <iostream>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <cstring>
#include <chrono>

class CircularBuffer {
public:
    explicit CircularBuffer(size_t size)
        : buffer_(size), capacity_(size), read_pos_(0), write_pos_(0), full_(false) {}

    void clear() {
        std::unique_lock<std::mutex> lock(mutex_);
        read_pos_ = 0;
        write_pos_ = 0;
        full_ = false;
        write_cv_.notify_all();
        read_cv_.notify_all();
        //std::cout << "Buffer cleared.\n";
    }

    int write(const char* data, size_t size, std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (!write_cv_.wait_for(lock, timeout, [this, size]() { return size <= availableSpace(); })) {
            std::cerr << "Write timeout.\n";
            return -1;
        }

        size_t end_space = capacity_ - write_pos_;
        if (size <= end_space) {
            std::memcpy(&buffer_[write_pos_], data, size);
        } else {
            std::memcpy(&buffer_[write_pos_], data, end_space);
            std::memcpy(&buffer_[0], data + end_space, size - end_space);
        }

        write_pos_ = (write_pos_ + size) % capacity_;
        full_ = (write_pos_ == read_pos_);
        read_cv_.notify_one();
        return size;
    }

    int read(char* data, size_t size, std::chrono::milliseconds timeout) {
        return accessData(data, size, timeout, false);
    }

    int peek(char* data, size_t size, std::chrono::milliseconds timeout) {
        return accessData(data, size, timeout, true);
    }

    size_t readableSize() const {
        std::unique_lock<std::mutex> lock(mutex_);
        return calculateReadableSize();
    }

private:
    int accessData(char* data, size_t size, std::chrono::milliseconds timeout, bool is_peek) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (!read_cv_.wait_for(lock, timeout, [this, size]() { return size <= calculateReadableSize(); })) {
            //std::cerr << (is_peek ? "Peek" : "Read") << " timeout.\n";
            return -1;
        }

        size_t end_space = capacity_ - read_pos_;
        if (size <= end_space) {
            std::memcpy(data, &buffer_[read_pos_], size);
        } else {
            std::memcpy(data, &buffer_[read_pos_], end_space);
            std::memcpy(data + end_space, &buffer_[0], size - end_space);
        }

        if (!is_peek) {
            read_pos_ = (read_pos_ + size) % capacity_;
            full_ = false;
            write_cv_.notify_one();
        }
        return size;
    }

    size_t calculateReadableSize() const {
        if (full_) return capacity_;
        if (write_pos_ >= read_pos_) return write_pos_ - read_pos_;
        return capacity_ - read_pos_ + write_pos_;
    }

    size_t availableSpace() const {
        return capacity_ - calculateReadableSize();
    }

    std::vector<char> buffer_;
    size_t capacity_;
    size_t read_pos_;
    size_t write_pos_;
    bool full_;
    mutable std::mutex mutex_;
    std::condition_variable read_cv_;
    std::condition_variable write_cv_;
};
