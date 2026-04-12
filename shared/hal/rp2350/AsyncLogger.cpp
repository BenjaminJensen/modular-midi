#include "AsyncLogger.h"


// Define the static member variable
bool Logger::initialized = false;

void Logger::init() {
    if (initialized) return;

    // Claim an unused hardware spinlock for the internal SEGGER RTT locks
    int lock_num = spin_lock_claim_unused(true);
    rtt_spin_lock = spin_lock_instance(lock_num);

    initialized = true;
}
// Define the global RTT spinlock pointer used by SEGGER_RTT_Conf.h
extern "C" {
    spin_lock_t* rtt_spin_lock = nullptr;
}