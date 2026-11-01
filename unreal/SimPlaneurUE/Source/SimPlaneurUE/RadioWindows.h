#pragma once
#include "FlightRadio.h"

// DirectInput exposes rotary controls and sliders beyond WinMM's six axes.
class FRadioWindows
{
public:
    FRadioWindows();
    ~FRadioWindows();
    void Enumerate(TArray<FRadioDevice>& Devices);
    bool Open(FRadioDevice& Device);
    void Close();
    bool Read(FRadioFrame& Frame);
private:
    struct FImpl;
    TUniquePtr<FImpl> Impl;
};
