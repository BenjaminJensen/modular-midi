#pragma once

#include "i_pin.h"

class FakePin : public IPin {
public:
    bool reading = false;

    bool read() override { return reading; }
};
