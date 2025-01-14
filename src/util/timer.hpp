#ifndef TIMER_HPP
#define TIMER_HPP

#include <iostream>
#include <boost/asio.hpp>
#include <functional>
#include <chrono>
#include <thread>

class Timer {
public:
    // 构造函数，接收io_context、超时时间、是否重复、回调函数
    Timer(boost::asio::io_context& io, int timeout_ms, bool repeat, std::function<void(int,int,std::thread::id)> callback)
        : io_(io), timer_(io), interval_(timeout_ms), repeat_(repeat), callback_(std::move(callback)), count_(0) {
        startTimer();  // 初始化时启动定时器
    }

    // 禁止拷贝构造和拷贝赋值
    Timer(const Timer&) = delete;
    Timer& operator=(const Timer&) = delete;

    // 移动构造函数
    Timer(Timer&& other) noexcept
        : io_(other.io_), timer_(std::move(other.timer_)), interval_(other.interval_), repeat_(other.repeat_),
          callback_(std::move(other.callback_)), count_(other.count_) {
        other.count_ = 0;  // 移动后原对象的状态清空
    }

    // 移动赋值运算符
    Timer& operator=(Timer&& other) noexcept {
        if (this != &other) {
            // io_context 不可以被赋值，保留原来的 io_context
            interval_ = other.interval_;
            repeat_ = other.repeat_;
            callback_ = std::move(other.callback_);
            count_ = other.count_;
            
            // 移动 timer_
            timer_ = std::move(other.timer_);

            other.count_ = 0;  // 移动后原对象的状态清空
        }
        return *this;
    }



    // 启动定时器
    void start() {
        startTimer();
    }

    // 停止定时器
    void stop() {
        timer_.cancel();
    }

    // 动态修改定时器的超时时间
    void setTimeout(int timeout_ms) {
        interval_ = timeout_ms;  // 更新超时时间
        startTimer();  // 重新启动定时器
    }

    void setRepeat(bool repeat) {
        repeat_ = repeat;
    }
private:
    // 启动定时器并安排下一次触发
    void startTimer() {
        timer_.expires_after(std::chrono::milliseconds(interval_));
        timer_.async_wait([this](const boost::system::error_code& ec) {
            if (!ec) {
                auto now = std::chrono::steady_clock::now();
                
                // 调用用户提供的回调函数
                if (callback_) {
                    callback_( count_,
                        std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count(),
                        std::this_thread::get_id());
                }

                ++count_;

                // 如果是重复定时器，则重新启动定时器
                if (repeat_) {
                    startTimer();  // 重复启动定时器
                }
            }
        });
    }

    boost::asio::io_context& io_;
    boost::asio::steady_timer timer_;
    int interval_;  // 超时时间（毫秒）
    bool repeat_;   // 是否重复执行
    std::function<void(int,int,std::thread::id)> callback_;  // 用户的回调函数
    int count_;     // 计数器
};


#endif