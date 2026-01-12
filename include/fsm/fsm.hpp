#pragma once
#include <string>
#include <mutex>
#include <memory>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <thread>
#include <functional>
#include <unordered_map>
#include <condition_variable>

namespace fsm {

enum class SystemState {
    IDLE,      
    MOVEIT_STARTING,      
    PLANNING,               
    EXECUTING,          
    OBSTACLE_DETECTED,          
    ERROR               
};

enum class SystemEvent {
    START_MOVEIT,     
    MOVEIT_READY,     
    MOVEIT_FAILED,    
    START_PLANNING,         
    PLANNING_SUCCESS,       
    PLANNING_FAILED,           
    EXECUTION_COMPLETE,    
    OBSTACLE_APPEARED,      
    OBSTACLE_CLEARED,           
    STOP_REQUEST,           
    ERROR_OCCURRED,         
    RESET_REQUEST           
};

class Fsm {
public:

    using StateChangeCallback = std::function<void(SystemState old_state, SystemState new_state)>;
    using EventCallback = std::function<bool(SystemEvent event)>;
    using LogCallback = std::function<void(const std::string& message)>;

    Fsm();
    virtual ~Fsm();

    bool initialize();
    void start();
    void stop();
    void reset();
    void shutdown();

    bool triggerEvent(SystemEvent event);
    bool triggerEvent(SystemEvent event, const std::string& data);

    SystemState getCurrentState() const;
    std::string getCurrentStateName() const;
    bool isInState(SystemState state) const;
    bool canTransition(SystemEvent event) const;

    bool waitForState(SystemState target_state, 
                        std::chrono::milliseconds timeout = std::chrono::milliseconds(0));

    void setStateChangeCallback(StateChangeCallback callback);
    void setEventCallback(SystemEvent event, EventCallback callback);
    void setLogCallback(LogCallback callback);

    void enableThreadSafety(bool enable);

    void logMessage(const std::string& message) const;

    static std::string stateToString(SystemState state);
    static std::string eventToString(SystemEvent event);

private:
    bool thread_safe_;
    bool running_;
    SystemState current_state_;
    SystemState previous_state_;

    mutable std::mutex mutex_;
    std::condition_variable cv_;

    std::unordered_map<SystemState, std::unordered_map<SystemEvent, SystemState>> transition_table_;

    StateChangeCallback state_change_callback_;
    std::unordered_map<SystemEvent, EventCallback> event_callbacks_;
    LogCallback log_callback_;

    bool has_original_trajectory_;
    std::string original_trajectory_data_;
    size_t execution_progress_;
    
    // 接收事件并判断能否执行转换。
    bool triggerEventInternal(SystemEvent event, const std::string& data);
    // 检查是否允许从当前状态触发该事件。
    bool canTransitionInternal(SystemEvent event) const;
    std::string getCurrentStateNameInternal() const;
    
    void setupTransitionTable();
    bool executeTransition(SystemState from, SystemState to, SystemEvent event);
    void onStateChanged(SystemState old_state, SystemState new_state); //触发外部状态变化回调。
    bool isValidTransition(SystemState from, SystemState to, SystemEvent event) const;

    void defaultLogCallback(const std::string& message);


    std::unique_ptr<std::unique_lock<std::mutex>> getOptionalLock() const;
};

} // namespace fsm