#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include "shared/render/label.h"

// Cross-core, latest-value-wins mailbox: one Label per display, double-buffered
// so a writer (RenderService, core 0) and the sole reader (Render Engine, core 1)
// never tear a read. Not a queue - no depth, no overflow policy. See docs/adr/0001.
template<uint8_t DisplayCount>
class Mailbox {
public:
    // Publishes label as the newest pending value for display_id, superseding
    // any not-yet-consumed previous value. Safe to call from the sole writer only.
    void write(uint8_t display_id, const Label& label) {
        Entry& entry = m_entries[display_id];
        uint8_t current = entry.active_index.load(std::memory_order_relaxed);
        uint8_t next = current ^ 1;
        entry.buffers[next] = label;
        entry.active_index.store(next, std::memory_order_release);
        entry.dirty.store(true, std::memory_order_release);
    }

    // If display_id has a pending update, fills out with it, clears the dirty
    // flag, and returns true. Returns false (out left untouched) otherwise.
    // Safe to call from the sole reader only.
    bool take_if_dirty(uint8_t display_id, Label& out) {
        Entry& entry = m_entries[display_id];
        if (!entry.dirty.exchange(false, std::memory_order_acquire)) {
            return false;
        }
        uint8_t current = entry.active_index.load(std::memory_order_acquire);
        out = entry.buffers[current];
        return true;
    }

private:
    struct Entry {
        std::array<Label, 2> buffers{};
        std::atomic<uint8_t> active_index{0};
        std::atomic<bool> dirty{false};
    };

    std::array<Entry, DisplayCount> m_entries{};
};
