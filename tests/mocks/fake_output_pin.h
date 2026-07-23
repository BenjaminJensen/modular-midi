#pragma once

class FakeOutputPin {
public:
    bool level = true;
    int write_count = 0;

    void write(bool new_level) {
        level = new_level;
        ++write_count;
    }
};
