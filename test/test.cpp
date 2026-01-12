#include "fsm/fsm.hpp"
#include <iostream>

int main() {
    fsm::Fsm state_machine;

    if (state_machine.initialize()) {
        std::cout << "State machine initialized successfully." << std::endl;
    }

    state_machine.start();
    std::cout << "Current state: " << state_machine.getCurrentStateName() << std::endl;

    state_machine.triggerEvent(fsm::SystemEvent::START_MOVEIT);
    std::cout << "State after START_MOVEIT: " << state_machine.getCurrentStateName() << std::endl;

    state_machine.triggerEvent(fsm::SystemEvent::MOVEIT_READY);
    std::cout << "State after MOVEIT_READY: " << state_machine.getCurrentStateName() << std::endl;

    state_machine.shutdown();
    return 0;
}