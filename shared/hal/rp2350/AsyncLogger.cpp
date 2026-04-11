#include "AsyncLogger.h"

// Define the static member variable
bool Logger::initialized = false;

// Define the global RTT spinlock pointer used by SEGGER_RTT_Conf.h
extern "C" {
    spin_lock_t* rtt_spin_lock = nullptr;
}