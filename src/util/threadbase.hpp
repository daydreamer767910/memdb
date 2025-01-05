#ifndef THREADBASE_HPP
#define THREADBASE_HPP

#include <iostream>
#include <string>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <variant>
#include <atomic>
#include <sstream>


template <typename... MSG>
class ThreadBase {
public:
    enum class task_status {
        INIT,
        IDLE,
        RUNNING,
        TERMINATE
    };
    // 定义输出运算符
    friend std::ostream& operator<<(std::ostream& os, const task_status& status) {
        static const char* status_strings[] = { "INIT", "IDLE", "RUNNING", "TERMINATE" };
        os << status_strings[static_cast<int>(status)];
        return os;
    }

    virtual ~ThreadBase(){
        terminate();
        if (this->thread_.joinable()) {
            this->thread_.join();  // 等待线程结束
		}
	}
    // 启动单线程,有可能运行2个任务:消息队列处理和定时器,2个任务下定时器会出现误差
    virtual void start() {
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
		    this->status_ = task_status::RUNNING;
        }
        //std::cout << "thread --> RUNNING\n";
		cond_var_.notify_one();
	};
    
    // 停止接口
    virtual void stop() {
		{
			std::lock_guard<std::mutex> lock(queue_mutex_);
			this->status_ = task_status::IDLE;
		}
        //std::cout << "thread --> IDLE\n";
        this->cond_var_.notify_one(); 
	}

    virtual void terminate() {
		{
			std::lock_guard<std::mutex> lock(queue_mutex_);
			this->status_ = task_status::TERMINATE;
		}
        std::cout << "thread --> TERMINATE\n";
        this->cond_var_.notify_one(); 
	}

    // 发送消息接口
    virtual void sendmsg(const std::shared_ptr<std::variant<MSG...>> msg) {
		{
			std::lock_guard<std::mutex> lock(queue_mutex_);
			msg_queue_.emplace(msg);
		}
		cond_var_.notify_one();
	}

    bool is_idle() {
        return task_status::IDLE == this->status_;
    }

    bool is_terminate() {
        return task_status::TERMINATE == this->status_;
    }

    bool is_running() {
        return task_status::RUNNING == this->status_;
    }

    std::string get_status() {
        std::ostringstream oss;
        oss << "status[" << this->status_ << "]";
        return oss.str(); 
    }
protected:
    ThreadBase() : status_(task_status::INIT) {
        this->thread_ = std::thread(&ThreadBase::process, this);
	}
    // 处理消息接口
    virtual void on_msg(const std::shared_ptr<std::variant<MSG...>> msg) {
        std::cout << "ThreadBase::on_msg ..." << std::endl;
    };

    virtual void process() {
        while (true) {
            std::unique_lock<std::mutex> lock(this->queue_mutex_);
            
            this->cond_var_.wait(lock, [this]() { 
                return !this->msg_queue_.empty() || this->is_terminate(); 
            });

            if (this->is_terminate()) {
                break;  // 退出线程
            } 
            // 如果有消息，处理消息
            if (!this->msg_queue_.empty()) {
                auto msg = this->msg_queue_.front();
                this->msg_queue_.pop();
                lock.unlock();  // 释放锁
                this->on_msg(msg);  // 处理消息
            }
        }
    }

private:
    // 成员变量
    std::queue<std::shared_ptr<std::variant<MSG...>>> msg_queue_;            // 消息队列
    std::mutex queue_mutex_;                  // 队列的互斥锁
    std::condition_variable cond_var_;       // 条件变量，用于线程同步
    std::thread thread_;                      // 线程
    task_status status_;                            
    
};

#endif