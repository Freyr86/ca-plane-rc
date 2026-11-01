#pragma once
#include "CoreMinimal.h"
#include "GliderControls.h"

class APlayerController;
class FRadioWindows;
struct FRadioDevice
{
    FString Key, LegacyKey, Name;
    TArray<int32> Axes;
    int32 Buttons=0, Hats=0;
    bool Transmitter=false;
};
struct FRadioFrame
{
    float Axes[8]={};
    TSet<FName> Down;
};
struct FControlBinding
{
    FName Plus, Minus;
    bool Inverted=false, Toggle=false;
};
struct FControlOutput
{
    float Value=0, Memory=0;
    bool WasPlus=false, WasMinus=false, Pressed=false, Prime=true;
};

struct FFlightRadio
{
    void Initialize();
    void Scan();
    void CycleDevice(int32 Step);
    void Poll(APlayerController* PC,bool InMenu);
    bool HasDevice() const {return Devices.IsValidIndex(Active);}
    bool EditingKeyboard() const {return Preferred==TEXT("Keyboard");}
    bool CanEdit() const {return EditingKeyboard() || HasDevice();}
    bool IsDriving() const {return HasDevice() && bMoved;}
    FString SourceLabel() const;
    FString Status() const;
    float RawAxis(int32 I) const {return Frame.Axes[I];}
    float Command(EGliderControl A) const;
    float SteeringCommand(bool UseAilerons) const;
    float Preview(EGliderControl A) const;
    bool Pressed(EGliderControl A) const;
    FString Shortcut(EGliderControl A) const;
    const FControlBinding& Binding(EGliderControl A) const;
    FString InputLabel(FName Key) const;
    bool SupportsMinus(EGliderControl A) const;
    void BeginLearn(EGliderControl A,bool Negative);
    void CancelLearn();
    void MenuKey(FName Key,bool Down);
    bool Learning() const {return LearnAction!=EGliderControl::Count;}
    bool HasConflict() const {return !Candidate.IsNone();}
    void Resolve(bool Move);
    void Clear(EGliderControl A,bool Negative);
    void Invert(EGliderControl A);
    void ToggleMode(EGliderControl A);
    bool CenterSticks();
    void LeaveMenu();
    FString Notice;

private:
    friend class FGliderBindingsTest;
    struct FProfile
    {
        FControlBinding Bindings[ControlCount];
        FControlOutput Outputs[ControlCount];
    };
    FProfile Keys, Stick;
    TSharedPtr<FRadioWindows> Backend;
    TArray<FRadioDevice> Devices;
    FString Preferred=TEXT("Auto"), SettingsIni;
    int32 Active=INDEX_NONE;
    FRadioFrame Frame, LearnStart;
    TSet<FName> MenuKeys;
    float Centers[8]={};
    bool bMoved=false, bNegative=false, HaveFrame=false, CaptureInverted=false;
    FName Candidate;
    EGliderControl LearnAction=EGliderControl::Count;
    FProfile& Edited() {return EditingKeyboard()?Keys:Stick;}
    const FProfile& Edited() const {return EditingKeyboard()?Keys:Stick;}
    void Select(const FString& KeepDevice=FString());
    void LoadProfile(FProfile& Profile,bool Keyboard);
    void SaveProfile();
    FString Section(bool Keyboard) const;
    void Detect(const FRadioFrame& Before);
    void Found(FName Source);
    void FeedFrame(const FRadioFrame& Next);
    void Evaluate(FProfile& Profile,const TSet<FName>& Down,bool Keyboard);
};
