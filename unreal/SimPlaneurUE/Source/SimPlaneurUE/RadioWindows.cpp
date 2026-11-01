#include "RadioWindows.h"
#if PLATFORM_WINDOWS
#include "Framework/Application/SlateApplication.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include "Windows/HideWindowsPlatformTypes.h"

struct FRadioWindows::FImpl
{
    TRefCountPtr<IDirectInput8W> Input;
    TRefCountPtr<IDirectInputDevice8W> Device;
    TMap<FString,GUID> Ids;
    TArray<int32> Axes;
    TArray<FName> ButtonNames;
    int32 Hats=0;
};
namespace
{
    const DWORD Offsets[]={DIJOFS_X,DIJOFS_Y,DIJOFS_Z,DIJOFS_RX,DIJOFS_RY,DIJOFS_RZ,DIJOFS_SLIDER(0),DIJOFS_SLIDER(1)};
    struct FEnumeration { TArray<FRadioDevice>& Devices; TMap<FString,GUID>& Ids; };
    BOOL CALLBACK DeviceFound(const DIDEVICEINSTANCEW* Info,void* Context)
    {
        auto& List=*static_cast<FEnumeration*>(Context);
        FRadioDevice D;
        D.Name=Info->tszProductName;
        const GUID& G=Info->guidInstance;
        D.Key=FString::Printf(TEXT("DI-%08X-%04X-%04X-"),G.Data1,G.Data2,G.Data3)+BytesToHex(G.Data4,8);
        const DWORD VidPid=Info->guidProduct.Data1;
        const FString Oem=FString::Printf(TEXT("VID_%04X&PID_%04X"),LOWORD(VidPid),HIWORD(VidPid));
        D.LegacyKey=FString::Printf(TEXT("Joy-%08x"),GetTypeHash(Oem));
        D.Transmitter=D.Name.Contains(TEXT("TX16")) || D.Name.Contains(TEXT("RadioMaster")) || D.Name.Contains(TEXT("EdgeTX"));
        List.Ids.Add(D.Key,G);
        List.Devices.Add(MoveTemp(D));
        return DIENUM_CONTINUE;
    }
}

FRadioWindows::FRadioWindows():Impl(MakeUnique<FImpl>())
{
    DirectInput8Create(GetModuleHandleW(nullptr),DIRECTINPUT_VERSION,IID_IDirectInput8W,(void**)Impl->Input.GetInitReference(),nullptr);
}
FRadioWindows::~FRadioWindows() {Close();}
void FRadioWindows::Close()
{
    if (Impl->Device) Impl->Device->Unacquire();
    Impl->Device.SafeRelease();
}
void FRadioWindows::Enumerate(TArray<FRadioDevice>& Devices)
{
    Devices.Reset();
    Impl->Ids.Reset();
    FEnumeration List{Devices,Impl->Ids};
    if (Impl->Input) Impl->Input->EnumDevices(DI8DEVCLASS_GAMECTRL,DeviceFound,&List,DIEDFL_ATTACHEDONLY);
    // Same-model handsets have distinct DirectInput instance IDs. Retain the old
    // WinMM profile suffix when upgrading users who owned two identical radios.
    TMap<FString,int32> Copies;
    for (auto& D:Devices)
    {
        const int32 N=++Copies.FindOrAdd(D.LegacyKey);
        if (N>1) D.LegacyKey+=FString::Printf(TEXT("-%d"),N);
    }
}
bool FRadioWindows::Open(FRadioDevice& D)
{
    Close();
    const GUID* G=Impl->Ids.Find(D.Key);
    if (!G || FAILED(Impl->Input->CreateDevice(*G,Impl->Device.GetInitReference(),nullptr))) return false;
    auto* Dev=Impl->Device.GetReference();
    const HWND Window=(HWND)FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);
    if (FAILED(Dev->SetDataFormat(&c_dfDIJoystick2)) || FAILED(Dev->SetCooperativeLevel(Window,DISCL_BACKGROUND|DISCL_NONEXCLUSIVE))) return false;
    DIDEVCAPS Caps{};
    Caps.dwSize=sizeof(Caps);
    if (FAILED(Dev->GetCapabilities(&Caps))) return false;
    D.Buttons=FMath::Min<int32>(Caps.dwButtons,128);
    Impl->Hats=D.Hats=FMath::Min<int32>(Caps.dwPOVs,4);
    D.Axes.Reset();
    for (int32 I=0;I<8;++I)
    {
        DIDEVICEOBJECTINSTANCEW Obj{}; Obj.dwSize=sizeof(Obj);
        if (FAILED(Dev->GetObjectInfo(&Obj,Offsets[I],DIPH_BYOFFSET))) continue;
        DIPROPRANGE Range{};
        Range.diph={sizeof(Range),sizeof(Range.diph),Offsets[I],DIPH_BYOFFSET};
        Range.lMin=-32767; Range.lMax=32767;
        if (SUCCEEDED(Dev->SetProperty(DIPROP_RANGE,&Range.diph))) D.Axes.Add(I);
    }
    Impl->Axes=D.Axes;
    Impl->ButtonNames.Reset();
    for (int32 I=0;I<D.Buttons;++I) Impl->ButtonNames.Emplace(*FString::Printf(TEXT("B%d"),I+1));
    return SUCCEEDED(Dev->Acquire());
}
bool FRadioWindows::Read(FRadioFrame& Out)
{
    auto* Dev=Impl->Device.GetReference();
    if (!Dev) return false;
    DIJOYSTATE2 State{};
    Dev->Poll();
    if (FAILED(Dev->GetDeviceState(sizeof(State),&State)))
    {
        if (FAILED(Dev->Acquire())) return false;
        Dev->Poll();
        if (FAILED(Dev->GetDeviceState(sizeof(State),&State))) return false;
    }
    const LONG Values[]={State.lX,State.lY,State.lZ,State.lRx,State.lRy,State.lRz,State.rglSlider[0],State.rglSlider[1]};
    for (int32 I:Impl->Axes) Out.Axes[I]=FMath::Clamp(Values[I]/32767.f,-1.f,1.f);
    for (int32 I=0;I<Impl->ButtonNames.Num();++I)
        if (State.rgbButtons[I]&0x80) Out.Down.Add(Impl->ButtonNames[I]);
    for (int32 I=0;I<Impl->Hats;++I)
        if (LOWORD(State.rgdwPOV[I])!=0xffff)
            Out.Down.Add(FName(*FString::Printf(TEXT("Hat%d.%d"),I+1,int((State.rgdwPOV[I]+2250)/4500)%8)));
    return true;
}
#else
struct FRadioWindows::FImpl {};
FRadioWindows::FRadioWindows()=default;
FRadioWindows::~FRadioWindows()=default;
void FRadioWindows::Close() {}
void FRadioWindows::Enumerate(TArray<FRadioDevice>& Devices) {Devices.Reset();}
bool FRadioWindows::Open(FRadioDevice&) {return false;}
bool FRadioWindows::Read(FRadioFrame&) {return false;}
#endif
