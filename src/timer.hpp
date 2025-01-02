#ifndef TIMER_HPP
#define TIMER_HPP

#include <iostream>
#include <uv.h>
#include <functional>

class Timer {
public:
    using TimerCallback = std::function<void()>;

    // 构造函数：初始化定时器
    Timer( uint64_t timeout, uint64_t repeat, TimerCallback callback)
        : timeout_(timeout), repeat_(repeat), callback_(std::move(callback)) {
        loop_ = uv_default_loop();
        // 初始化 timer handle
        timer_ = new uv_timer_t();
        uv_timer_init(loop_, timer_);
        timer_->data = this; // 关联 timer handle 和当前对象

        // 启动定时器
        uv_timer_start(timer_, Timer::onTimerCallback, timeout_, repeat_);
    }

    // 禁止拷贝
    Timer(const Timer&) = delete;
    Timer& operator=(const Timer&) = delete;

    // 移动构造函数
    Timer(Timer&& other) noexcept
        : loop_(other.loop_), timer_(other.timer_), callback_(std::move(other.callback_)),
          timeout_(other.timeout_), repeat_(other.repeat_) {
        other.timer_ = nullptr;
    }

    // 移动赋值运算符
    Timer& operator=(Timer&& other) noexcept {
        if (this != &other) {
            stop();
            loop_ = other.loop_;
            timer_ = other.timer_;
            callback_ = std::move(other.callback_);
            timeout_ = other.timeout_;
            repeat_ = other.repeat_;
            other.timer_ = nullptr;
        }
        return *this;
    }

    // 启动定时器
    void start() {
        if (!timer_) {
            timer_ = new uv_timer_t();
            uv_timer_init(loop_, timer_);
            timer_->data = this;
        }
        uv_timer_start(timer_, Timer::onTimerCallback, timeout_, repeat_);
    }
    
    // 停止定时器
    void stop() {
        if (timer_) {
            uv_timer_stop(timer_);
            uv_close(reinterpret_cast<uv_handle_t*>(timer_), Timer::onCloseCallback);
            timer_ = nullptr;
        }
    }

    // 析构函数：释放资源
    ~Timer() {
        stop();
    }

private:
    uv_loop_t* loop_;
    uv_timer_t* timer_;
    TimerCallback callback_;
    uint64_t timeout_;
    uint64_t repeat_;

    // 定时器触发回调（静态方法）
    static void onTimerCallback(uv_timer_t* handle) {
        auto* self = static_cast<Timer*>(handle->data);
        if (self->callback_) {
            self->callback_();
        }
    }

    // 关闭定时器时的回调
    static void onCloseCallback(uv_handle_t* handle) {
        delete reinterpret_cast<uv_timer_t*>(handle);
    }
};


#endif