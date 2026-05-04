
#pragma once

class IPin {
public:
    // 'virtual' tells the compiler to look this up at runtime
    // '= 0' makes it a "pure virtual" (it must be implemented by someone)
    // Must return true if the pin is HIGH, false if LOW
    virtual bool read() = 0;
};