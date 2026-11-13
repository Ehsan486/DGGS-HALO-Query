#include "bafe_exp/process_control.hpp"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace bafe_exp {

std::string stabilize_process() {
#ifdef _WIN32
    const HANDLE process = GetCurrentProcess();
    const bool priority = SetPriorityClass(process, HIGH_PRIORITY_CLASS) != 0;
    bool throttling_off = false;
#ifdef PROCESS_POWER_THROTTLING_CURRENT_VERSION
    PROCESS_POWER_THROTTLING_STATE state{};
    state.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
    state.ControlMask = PROCESS_POWER_THROTTLING_EXECUTION_SPEED;
    state.StateMask = 0;  // controlled and off: the process is never placed in the throttled class
    throttling_off = SetProcessInformation(process, ProcessPowerThrottling, &state, sizeof(state)) != 0;
#endif
    return std::string("process: priority_high=") + (priority ? "1" : "0") +
           " power_throttling_off=" + (throttling_off ? "1" : "0");
#else
    return "process: default";
#endif
}

}  // namespace bafe_exp
