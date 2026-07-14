#pragma once

class FakePin {
public:
    bool reading = false;

    bool read() { return reading; }
};
