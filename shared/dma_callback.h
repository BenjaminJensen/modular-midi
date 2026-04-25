#pragma once

class DMA_Callback {
public:
    // 'virtual' tells the compiler to look this up at runtime
    // '= 0' makes it a "pure virtual" (it must be implemented by someone)
    virtual void on_dma_complete() = 0;
};