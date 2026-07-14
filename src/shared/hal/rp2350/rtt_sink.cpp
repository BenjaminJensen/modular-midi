#include "rtt_sink.h"

#include "pico/stdlib.h"
#include "SEGGER_RTT.h"

void RttSink::init() {
    if (m_lock) return;
    int lock_num = spin_lock_claim_unused(true);
    m_lock = spin_lock_instance(lock_num);
}

void RttSink::write(std::string_view text) {
    if (!m_lock) return;

    uint32_t save = spin_lock_blocking(m_lock);

    SEGGER_RTT_WriteString(0, get_core_num() == 0 ? "0: " : "1: ");
    SEGGER_RTT_Write(0, text.data(), text.size());
    SEGGER_RTT_WriteString(0, "\r\n");

    spin_unlock(m_lock, save);
}
