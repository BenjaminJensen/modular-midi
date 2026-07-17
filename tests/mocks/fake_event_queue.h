#pragma once

#include <vector>
#include "shared/event/event_common.h"

// Host-side EventQueue double: records every sent Event instead of bounding
// capacity. Overflow/drop-newest behavior belongs to the real (capacity-limited)
// EventQueue implementation and is tested there, not re-verified through a
// producer's tests.
class FakeEventQueue {
public:
    std::vector<Event> sent;

    bool send(const Event& item) {
        sent.push_back(item);
        return true;
    }

    bool receive(Event& out, uint32_t /*timeout_ms*/) {
        if (sent.empty()) {
            return false;
        }
        out = sent.front();
        sent.erase(sent.begin());
        return true;
    }
};
