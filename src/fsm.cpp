#include "fsm/fsm.hpp"

namespace fsm {

// 初始化状态机对象，设置默认状态为 IDLE，并初始化其他成员变量。
Fsm::Fsm()
    : thread_safe_(false), running_(false),  // 默认为未运行,非线程安全
    current_state_(SystemState::IDLE), // 初始状态设为 IDLE
    previous_state_(SystemState::IDLE), // 默认状态设为 IDLE

    execution_progress_(0) {
    log_callback_ = [this](const std::string& msg) { defaultLogCallback(msg); };
    setupTransitionTable(); // 初始化状态转换表。

}

// 析构函数，确保状态机在销毁时调用 shutdown() 释放资源。
Fsm::~Fsm() {
    shutdown();
}

// 初始化状态机，设置当前状态为 IDLE，并初始化其他成员变量。
bool Fsm::initialize() {
    auto lock = getOptionalLock();
    
    current_state_ = SystemState::IDLE;
    previous_state_ = SystemState::IDLE;
    running_ = true;
    
    execution_progress_ = 0;

    logMessage("状态机初始化");
    return true;
}

// 启动状态机，将状态设置为 IDLE 并标记为运行中。
void Fsm::start() {
    auto lock = getOptionalLock();
    
    logMessage("启动状态机");
    running_ = true;
    current_state_ = SystemState::IDLE;
}

// 停止状态机，触发 STOP_REQUEST 事件。
void Fsm::stop() {
    triggerEvent(SystemEvent::STOP_REQUEST);
}

// 重置状态机，将当前状态设置为 IDLE，并调用 onStateChanged() 显示状态变化。
void Fsm::reset() {
    auto lock = getOptionalLock();
    
    logMessage("重置状态机");
    previous_state_ = current_state_;
    current_state_ = SystemState::IDLE;

    onStateChanged(previous_state_, current_state_);
}

// 关闭状态机，标记为未运行并通知所有等待的线程。
void Fsm::shutdown() {
    auto lock = getOptionalLock();
    
    logMessage("关闭状态机");
    running_ = false;
    
    cv_.notify_all();
}

// 触发一个事件，进行状态转换。
bool Fsm::triggerEvent(SystemEvent event) {
    return triggerEvent(event, "");
}

// 触发一个事件，并附带额外数据。
bool Fsm::triggerEvent(SystemEvent event, const std::string& data) {
    auto lock = getOptionalLock();
    return triggerEventInternal(event, data);
}

// 执行状态转换。
bool Fsm::triggerEventInternal(SystemEvent event, const std::string& data) {
    (void)data; // 当前未使用附加数据参数，避免编译警告。
    auto state_it = transition_table_.find(current_state_);
    if (state_it == transition_table_.end()) {
        logMessage("未找到状态 " + getCurrentStateNameInternal() + " 的转换表");
        return false;
    }

    auto event_it = state_it->second.find(event);
    if (event_it == state_it->second.end()) {
        logMessage("状态 " + getCurrentStateNameInternal() + " 不支持事件 " + eventToString(event));
        return false;
    }

    SystemState target_state = event_it->second;

    // 执行事件回调
    if (event_callbacks_.find(event) != event_callbacks_.end()) {
        try {
            if (!event_callbacks_[event](event)) {
                logMessage("Event callbacks failed: " + eventToString(event));
                return false;
            }
        } catch (const std::exception& e) {
            logMessage("Event callbacks are abnormal: " + std::string(e.what()));
            return false;
        }
    }

    return executeTransition(current_state_, target_state, event);
}

// 检查当前状态是否可以通过指定事件进行转换。
bool Fsm::canTransition(SystemEvent event) const {
    auto lock = getOptionalLock();
    return canTransitionInternal(event);
}

bool Fsm::canTransitionInternal(SystemEvent event) const {

    auto state_it = transition_table_.find(current_state_);

    if (state_it == transition_table_.end()) return false;

    auto event_it = state_it->second.find(event);
    return (event_it != state_it->second.end());
}

// 获取当前状态
SystemState Fsm::getCurrentState() const {
    auto lock = getOptionalLock();
    return current_state_;
}

// 获取当前状态的名称（字符串形式）。
std::string Fsm::getCurrentStateName() const {
    auto lock = getOptionalLock();
    return getCurrentStateNameInternal();
}

std::string Fsm::getCurrentStateNameInternal() const {
    return stateToString(current_state_);
}

// 检查当前状态是否为指定状态。
bool Fsm::isInState(SystemState state) const {
    return getCurrentState() == state;
}

// 设置状态改变回调函数。
void Fsm::setStateChangeCallback(StateChangeCallback callback) {
    auto lock = getOptionalLock();
    state_change_callback_ = callback;
    logMessage("State change callback registered");
}

// 为指定事件设置回调函数。
void Fsm::setEventCallback(SystemEvent event, EventCallback callback) {
    auto lock = getOptionalLock();
    event_callbacks_[event] = callback;
    logMessage("Register the event callback: " + eventToString(event));
}

// 设置日志回调函数。
void Fsm::setLogCallback(LogCallback callback) {
    auto lock = getOptionalLock();
    log_callback_ = callback;
}

// 等待状态机进入指定状态，支持超时。
bool Fsm::waitForState(SystemState target_state, std::chrono::milliseconds timeout) {
    if (!thread_safe_) {
        logMessage("The wait state feature requires thread safety to be enabled");
        return false;
    }

    std::unique_lock<std::mutex> lock(mutex_);
    
    if (current_state_ == target_state) {
        return true;
    }

    if (timeout.count() > 0) {
        return cv_.wait_for(lock, timeout, [this, target_state]() {
            return current_state_ == target_state || !running_;
        });
    } else {
        cv_.wait(lock, [this, target_state]() {
            return current_state_ == target_state || !running_;
        });
        return current_state_ == target_state;
    }
}

// 执行状态转换。
bool Fsm::executeTransition(SystemState from, SystemState to, SystemEvent event) {

    if (!isValidTransition(from, to, event)) {
        logMessage("无效转换: " + stateToString(from) + " -> " + stateToString(to));
        return false;
    }
    
    previous_state_ = current_state_;
    current_state_ = to;

    logMessage("状态转换: " + stateToString(from) + " -> " + stateToString(to));
    
    onStateChanged(previous_state_, current_state_);

    return true;
}

// 在状态变化时调用，触发回调函数并通知等待线程。
void Fsm::onStateChanged(SystemState old_state, SystemState new_state) {

    if (state_change_callback_) {
        try {
            state_change_callback_(old_state, new_state);
        } catch (const std::exception& e) {
            logMessage("状态变化回调异常: " + std::string(e.what()));
        }
    }

    cv_.notify_all();
}

// 检查状态转换是否有效。
bool Fsm::isValidTransition(SystemState from, SystemState to, SystemEvent event) const {

    auto state_it = transition_table_.find(from);
    if (state_it == transition_table_.end()) {
        return false;
    }

    auto event_it = state_it->second.find(event);
    if (event_it == state_it->second.end()) {
        return false;
    }

    return event_it->second == to;
}

// 打印日志。
void Fsm::logMessage(const std::string& message) const {
    if (log_callback_) {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

        std::stringstream ss;
        ss << "[" << std::put_time(std::localtime(&time_t), "%H:%M:%S");
        ss << "." << std::setfill('0') << std::setw(3) << ms.count() << "] ";
        ss << message;

        log_callback_(ss.str());
    }
}

// 默认日志回调函数，将日志输出到控制台。
void Fsm::defaultLogCallback(const std::string& message) {
    std::cout << "[FSM] " << message << std::endl;
}

// 设置是否启用线程安全。
void Fsm::enableThreadSafety(bool enable) {
    thread_safe_ = enable;
}
std::unique_ptr<std::unique_lock<std::mutex>> Fsm::getOptionalLock() const {
    if (thread_safe_) 
        return std::make_unique<std::unique_lock<std::mutex>>(mutex_);
    else
        return nullptr;
}

//初始化状态转换表，定义状态与事件的关系。
void Fsm::setupTransitionTable() {

    transition_table_[SystemState::IDLE][SystemEvent::START_MOVEIT] = SystemState::MOVEIT_STARTING;
    transition_table_[SystemState::IDLE][SystemEvent::RESET_REQUEST] = SystemState::IDLE;
    transition_table_[SystemState::IDLE][SystemEvent::ERROR_OCCURRED] = SystemState::ERROR;
    
    transition_table_[SystemState::MOVEIT_STARTING][SystemEvent::MOVEIT_READY] = SystemState::PLANNING;
    transition_table_[SystemState::MOVEIT_STARTING][SystemEvent::MOVEIT_FAILED] = SystemState::ERROR;
    transition_table_[SystemState::MOVEIT_STARTING][SystemEvent::ERROR_OCCURRED] = SystemState::ERROR;
    transition_table_[SystemState::MOVEIT_STARTING][SystemEvent::STOP_REQUEST] = SystemState::IDLE;

    transition_table_[SystemState::PLANNING][SystemEvent::PLANNING_SUCCESS] = SystemState::EXECUTING;
    transition_table_[SystemState::PLANNING][SystemEvent::PLANNING_FAILED] = SystemState::ERROR;
    transition_table_[SystemState::PLANNING][SystemEvent::ERROR_OCCURRED] = SystemState::ERROR;
    transition_table_[SystemState::PLANNING][SystemEvent::OBSTACLE_APPEARED] = SystemState::OBSTACLE_DETECTED;
    transition_table_[SystemState::PLANNING][SystemEvent::STOP_REQUEST] = SystemState::IDLE;

    transition_table_[SystemState::EXECUTING][SystemEvent::EXECUTION_COMPLETE] = SystemState::IDLE;
    transition_table_[SystemState::EXECUTING][SystemEvent::OBSTACLE_APPEARED] = SystemState::OBSTACLE_DETECTED;
    transition_table_[SystemState::EXECUTING][SystemEvent::STOP_REQUEST] = SystemState::IDLE;
    transition_table_[SystemState::EXECUTING][SystemEvent::ERROR_OCCURRED] = SystemState::ERROR;

    transition_table_[SystemState::OBSTACLE_DETECTED][SystemEvent::START_PLANNING] = SystemState::PLANNING;
    transition_table_[SystemState::OBSTACLE_DETECTED][SystemEvent::STOP_REQUEST] = SystemState::IDLE;
    transition_table_[SystemState::OBSTACLE_DETECTED][SystemEvent::ERROR_OCCURRED] = SystemState::ERROR;

    transition_table_[SystemState::ERROR][SystemEvent::RESET_REQUEST] = SystemState::IDLE;
    transition_table_[SystemState::ERROR][SystemEvent::STOP_REQUEST] = SystemState::IDLE;

}

// 状态定义（SystemState 枚举）
std::string Fsm::stateToString(SystemState state) {
    switch (state) {
        case SystemState::IDLE:              return "IDLE";
        case SystemState::MOVEIT_STARTING:   return "MOVEIT_STARTING";
        case SystemState::PLANNING:          return "PLANNING";
        case SystemState::EXECUTING:         return "EXECUTING";
        case SystemState::OBSTACLE_DETECTED: return "OBSTACLE_DETECTED";
        case SystemState::ERROR:             return "ERROR";
        default:                             return "UNKNOWN";
    }
}

//事件定义（SystemEvent 枚举）
std::string Fsm::eventToString(SystemEvent event) {
    switch (event) {
        case SystemEvent::START_MOVEIT:        return "START_MOVEIT";
        case SystemEvent::MOVEIT_READY:        return "MOVEIT_READY";
        case SystemEvent::MOVEIT_FAILED:       return "MOVEIT_FAILED";
        case SystemEvent::START_PLANNING:      return "START_PLANNING";
        case SystemEvent::PLANNING_SUCCESS:    return "PLANNING_SUCCESS";
        case SystemEvent::PLANNING_FAILED:     return "PLANNING_FAILED";
        case SystemEvent::EXECUTION_COMPLETE:  return "EXECUTION_COMPLETE";
        case SystemEvent::OBSTACLE_APPEARED:   return "OBSTACLE_APPEARED";
        case SystemEvent::OBSTACLE_CLEARED:    return "OBSTACLE_CLEARED";
        case SystemEvent::STOP_REQUEST:        return "STOP_REQUEST";
        case SystemEvent::ERROR_OCCURRED:      return "ERROR_OCCURRED";
        case SystemEvent::RESET_REQUEST:       return "RESET_REQUEST";
        default:                               return "UNKNOWN_EVENT";
    }
}

} // namespace fsm