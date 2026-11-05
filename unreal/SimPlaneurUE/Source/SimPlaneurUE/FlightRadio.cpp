#include "FlightRadio.h"
#include "PilotMovement.h"
#include "Algo/AnyOf.h"
#include "RadioWindows.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Misc/ConfigCacheIni.h"

namespace
{
    using A=EGliderControl;
    using T=EControlType;
    const TCHAR* Root=TEXT("SimPlaneur.Radio");
    const FName Axes[]={TEXT("AxisX"),TEXT("AxisY"),TEXT("AxisZ"),TEXT("AxisRx"),TEXT("AxisRy"),TEXT("AxisRz"),TEXT("Slider1"),TEXT("Slider2")};
    const TCHAR* AxisLabels[]={TEXT("Axe X"),TEXT("Axe Y"),TEXT("Axe Z"),TEXT("Rotation X"),TEXT("Rotation Y"),TEXT("Rotation Z"),TEXT("Curseur 1"),TEXT("Curseur 2")};
    constexpr int32 OldAxes[]={0,1,2,5,3,4};
    int32 AxisIndex(FName Key)
    {
        for (int32 I=0;I<8;++I) if (Axes[I]==Key) return I;
        return INDEX_NONE;
    }
    float Centered(float V,float Center)
    {
        Center=FMath::Clamp(Center,-.45f,.45f);
        return FMath::Clamp((V-Center)/(V>=Center?1.f-Center:1.f+Center),-1.f,1.f);
    }
    float StickCurve(float V)
    {
        return FMath::Sign(V)*FMath::Pow(FMath::Clamp((FMath::Abs(V)-.035f)/.965f,0.f,1.f),1.35f);
    }
}

void FFlightRadio::Initialize()
{
    SettingsIni=GGameUserSettingsIni;
    Backend=MakeShared<FRadioWindows>();
    GConfig->GetString(Root,TEXT("Preferred"),Preferred,SettingsIni);
    LoadProfile(Keys,true);
    Scan();
}

void FFlightRadio::Scan()
{
    const FString Previous=HasDevice()?Devices[Active].Key:FString();
    Backend->Enumerate(Devices);
    Select(Previous);
}
void FFlightRadio::Select(const FString& KeepDevice)
{
    CancelLearn();
    Backend->Close();
    if (Preferred==TEXT("Auto"))
    {
        Active=Devices.IndexOfByPredicate([](const FRadioDevice& D){return D.Name.Contains(TEXT("TX16"));});
        if (Active==INDEX_NONE) Active=Devices.IndexOfByPredicate([](const FRadioDevice& D){return D.Transmitter;});
        if (Active==INDEX_NONE && Devices.Num()==1) Active=0;
    }
    else Active=Devices.IndexOfByPredicate([this](const FRadioDevice& D){return D.Key==Preferred || D.LegacyKey==Preferred;});
    if (HasDevice() && !Backend->Open(Devices[Active])) {Backend->Close();Active=INDEX_NONE;}
    HaveFrame=false;
    if (HasDevice() && Devices[Active].Key==KeepDevice)
    {
        for (auto& Out:Stick.Outputs) {Out.Prime=true;Out.Pressed=false;}
        return;
    }
    Frame={}; bMoved=false;
    for (float& C:Centers) C=0;
    LoadProfile(Stick,false);
    if (HasDevice())
        UE_LOG(LogTemp,Display,TEXT("RC inputs: %s / %d axes, %d buttons, %d POV"),*Devices[Active].Name,Devices[Active].Axes.Num(),Devices[Active].Buttons,Devices[Active].Hats);
}
void FFlightRadio::CycleDevice(int32 Step)
{
    TArray<FString> Options={TEXT("Auto"),TEXT("Keyboard")};
    for (const auto& D:Devices) Options.Add(D.Key);
    if (HasDevice() && Devices[Active].LegacyKey==Preferred) Preferred=Devices[Active].Key;
    Options.AddUnique(Preferred);
    Preferred=Options[(Options.IndexOfByKey(Preferred)+Step+Options.Num())%Options.Num()];
    Select();
    GConfig->SetString(Root,TEXT("Preferred"),*Preferred,SettingsIni);
    GConfig->Flush(false,SettingsIni);
}
FString FFlightRadio::Section(bool Keyboard) const
{
    return FString(Root)+TEXT(".")+(Keyboard?FString(TEXT("Keyboard")):Devices[Active].Key);
}
void FFlightRadio::LoadProfile(FProfile& P,bool Keyboard)
{
    P={};
    if (!Keyboard && !HasDevice()) return;
    const FString Sec=Section(Keyboard);
    if (!Keyboard)
    {
        const FString Old=FString(Root)+TEXT(".")+Devices[Active].LegacyKey;
        for (int32 I=0;I<2;++I)
        {
            int32 Slot=I;
            GConfig->GetInt(*Old,*FString::Printf(TEXT("Axis%d"),I),Slot,SettingsIni);
            const int32 Axis=OldAxes[FMath::Clamp(Slot,0,5)];
            if (Devices[Active].Axes.Contains(Axis)) P.Bindings[I].Plus=Axes[Axis];
            P.Bindings[I].Inverted=I==1;
            GConfig->GetBool(*Old,*FString::Printf(TEXT("Invert%d"),I),P.Bindings[I].Inverted,SettingsIni);
        }
        for (int32 I=0;I<6;++I) GConfig->GetFloat(*Old,*FString::Printf(TEXT("Center%d"),I),Centers[OldAxes[I]],SettingsIni);
        for (int32 I=0;I<8;++I) GConfig->GetFloat(*Sec,*Axes[I].ToString(),Centers[I],SettingsIni);
    }
    for (int32 I=0;I<ControlCount;++I)
    {
        const auto& S=ControlSpec(A(I));
        FControlBinding& B=P.Bindings[I];
        if (Keyboard) {if (S.KeyPlus) B.Plus=FName(S.KeyPlus);if (S.KeyMinus) B.Minus=FName(S.KeyMinus);}
        FString Saved;
        TArray<FString> F;
        if (GConfig->GetString(*Sec,S.Id,Saved,SettingsIni) && Saved.ParseIntoArray(F,TEXT("|"),false)==4)
            B={FName(*F[0]),FName(*F[1]),F[2]==TEXT("1"),F[3]==TEXT("1")};
    }
    // Migrate only the former stock shortcuts once. Custom bindings remain intact.
    int32 ShortcutVersion = 0;
    if (Keyboard && (!GConfig->GetInt(*Sec,TEXT("ShortcutVersion"),ShortcutVersion,SettingsIni) || ShortcutVersion < 1))
    {
        struct FOldKey { A Action; const TCHAR* Key; };
        const FOldKey OldKeys[] = {{A::Relaunch,TEXT("R")},{A::Camera,TEXT("C")},
            {A::WindTrails,TEXT("F")},{A::Noclip,TEXT("N")},{A::Vario,TEXT("K")}};
        for (const auto& Old : OldKeys)
        {
            auto& Binding = P.Bindings[int32(Old.Action)];
            const FName Target(ControlSpec(Old.Action).KeyPlus);
            const bool Occupied = Algo::AnyOf(P.Bindings,[&](const FControlBinding& Other)
                { return &Other != &Binding && (Other.Plus == Target || Other.Minus == Target); });
            if (Binding.Plus != FName(Old.Key) || Occupied) continue;
            Binding.Plus = Target;
            const FString Saved = FString::Printf(TEXT("%s|%s|%d|%d"),*Binding.Plus.ToString(),*Binding.Minus.ToString(),Binding.Inverted,Binding.Toggle);
            GConfig->SetString(*Sec,ControlSpec(Old.Action).Id,*Saved,SettingsIni);
        }
        GConfig->SetInt(*Sec,TEXT("ShortcutVersion"),1,SettingsIni);
        GConfig->Flush(false,SettingsIni);
    }
    // Retire the diagnostic panel binding, including previously saved F1/radio
    // assignments. F1 now selects the winch.
    P.Bindings[int32(A::Instruments)]={};
    if(GConfig->RemoveKey(*Sec,ControlSpec(A::Instruments).Id,SettingsIni)) GConfig->Flush(false,SettingsIni);
}
void FFlightRadio::SaveProfile()
{
    if (!CanEdit()) return;
    const FString Sec=Section(EditingKeyboard());
    for (int32 I=0;I<ControlCount;++I)
    {
        const auto& B=Edited().Bindings[I];
        const FString S=FString::Printf(TEXT("%s|%s|%d|%d"),*B.Plus.ToString(),*B.Minus.ToString(),B.Inverted,B.Toggle);
        GConfig->SetString(*Sec,ControlSpec(A(I)).Id,*S,SettingsIni);
    }
    if (!EditingKeyboard())
        for (int32 I=0;I<8;++I) GConfig->SetFloat(*Sec,*Axes[I].ToString(),Centers[I],SettingsIni);
    GConfig->Flush(false,SettingsIni);
}

const FControlBinding& FFlightRadio::Binding(A Action) const {return Edited().Bindings[int32(Action)];}
bool FFlightRadio::SupportsMinus(A Action) const
{
    const T Type=ControlSpec(Action).Type;
    return Type!=T::Switch && Type!=T::Pulse && AxisIndex(Binding(Action).Plus)==INDEX_NONE;
}
FString FFlightRadio::SourceLabel() const
{
    if (Preferred==TEXT("Auto")) return TEXT("Automatique");
    if (EditingKeyboard()) return TEXT("Clavier");
    return HasDevice()?Devices[Active].Name:TEXT("Radio débranchée");
}
FString FFlightRadio::Status() const
{
    if (!HasDevice()) return EditingKeyboard()?TEXT("Clavier actif"):TEXT("Aucune radio active");
    const auto& D=Devices[Active];
    return FString::Printf(TEXT("%s · %d axes · %d boutons"),*D.Name,D.Axes.Num(),D.Buttons);
}
FString FFlightRadio::InputLabel(FName Key) const
{
    if (Key.IsNone()) return TEXT("Libre");
    if (EditingKeyboard()) return FKey(Key).GetDisplayName().ToString();
    const int32 Axis=AxisIndex(Key);
    if (Axis>=0) return AxisLabels[Axis];
    const FString Id=Key.ToString();
    if (Id.StartsWith(TEXT("B"))) return TEXT("Bouton ")+Id.Mid(1);
    FString Hat,Direction;
    if (Id.Split(TEXT("."),&Hat,&Direction))
    {
        static const TCHAR* Dirs[]={TEXT("N"),TEXT("NE"),TEXT("E"),TEXT("SE"),TEXT("S"),TEXT("SO"),TEXT("O"),TEXT("NO")};
        return TEXT("Chapeau ")+Hat.Mid(3)+TEXT(" ")+Dirs[FMath::Clamp(FCString::Atoi(*Direction),0,7)];
    }
    return Id;
}
bool FFlightRadio::CenterSticks()
{
    if (!HasDevice() || !HaveFrame) return false;
    TArray<int32> Used;
    for (A Action:{A::Rudder,A::Elevator,A::Aileron})
    {
        const int32 I=AxisIndex(Binding(Action).Plus);
        if (I<0) continue;
        if (FMath::Abs(Frame.Axes[I])>.45f) return false;
        Used.AddUnique(I);
    }
    for (int32 I:Used) Centers[I]=Frame.Axes[I];
    if (Used.IsEmpty()) return false;
    SaveProfile(); return true;
}

void FFlightRadio::CancelLearn() {LearnAction=A::Count; Candidate=NAME_None; Notice.Empty();}
void FFlightRadio::BeginLearn(A Action,bool Minus)
{
    CancelLearn();
    if (!CanEdit() || (Minus && !SupportsMinus(Action))) return;
    LearnAction=Action; bNegative=Minus; CaptureInverted=false; LearnStart=Frame;
    Notice=EditingKeyboard()?TEXT("Appuie sur la touche voulue."):Minus?TEXT("Actionne un bouton ou un chapeau."):TEXT("Bouge la commande vers son sens positif.");
}
void FFlightRadio::Found(FName Source)
{
    Candidate=Source;
    for (int32 I=0;I<ControlCount;++I)
    {
        const auto& B=Edited().Bindings[I];
        if (I!=int32(LearnAction) && (Source==B.Plus || Source==B.Minus))
        {
            Notice=InputLabel(Source)+TEXT(" est déjà lié à « ")+ControlSpec(A(I)).Label+TEXT(" ».");
            return;
        }
    }
    Resolve(false);
}
void FFlightRadio::Resolve(bool Move)
{
    if (!Learning() || Candidate.IsNone() || !CanEdit()) return;
    auto& P=Edited();
    if (Move)
        for (int32 I=0;I<ControlCount;++I)
        {
            auto& B=P.Bindings[I];
            if (I==int32(LearnAction) || (B.Plus!=Candidate && B.Minus!=Candidate)) continue;
            if (B.Plus==Candidate) B.Plus=NAME_None;
            if (B.Minus==Candidate) B.Minus=NAME_None;
            P.Outputs[I]={};
        }
    auto& B=P.Bindings[int32(LearnAction)];
    FName& Target=bNegative?B.Minus:B.Plus;
    FName& Other=bNegative?B.Plus:B.Minus;
    Target=Candidate;
    if (Other==Candidate || AxisIndex(Candidate)>=0) Other=NAME_None;
    if (!bNegative) B.Inverted=CaptureInverted;
    P.Outputs[int32(LearnAction)]={};
    const FString Done=InputLabel(Candidate)+TEXT(" → ")+ControlSpec(LearnAction).Label;
    CancelLearn(); Notice=Done;
    SaveProfile();
}
void FFlightRadio::Clear(A Action,bool Minus)
{
    CancelLearn();
    auto& B=Edited().Bindings[int32(Action)];
    (Minus?B.Minus:B.Plus)=NAME_None;
    Edited().Outputs[int32(Action)]={};
    SaveProfile();
}
void FFlightRadio::Invert(A Action)
{
    auto& B=Edited().Bindings[int32(Action)];
    B.Inverted=!B.Inverted; Edited().Outputs[int32(Action)]={}; SaveProfile();
}
void FFlightRadio::ToggleMode(A Action)
{
    auto& B=Edited().Bindings[int32(Action)];
    B.Toggle=!B.Toggle; Edited().Outputs[int32(Action)]={}; SaveProfile();
}
void FFlightRadio::MenuKey(FName Key,bool Down)
{
    if (!Down) {MenuKeys.Remove(Key);return;}
    if (!MenuKeys.Contains(Key) && EditingKeyboard() && Learning() && !HasConflict()) Found(Key);
    MenuKeys.Add(Key);
}
void FFlightRadio::LeaveMenu()
{
    CancelLearn();MenuKeys.Reset();
    for (auto* P:{&Stick,&Keys})
        for (auto& O:P->Outputs) {O.Prime=true;O.Pressed=false;}
}
void FFlightRadio::Detect(const FRadioFrame& Before)
{
    if (!Learning() || EditingKeyboard() || HasConflict()) return;
    for (FName Key:Frame.Down)
        if (!Before.Down.Contains(Key)) {Found(Key);return;}
    if (!bNegative)
    {
        int32 Axis=INDEX_NONE; float Largest=.32f;
        for (int32 I:Devices[Active].Axes)
        {
            const float Delta=Frame.Axes[I]-LearnStart.Axes[I];
            if (FMath::Abs(Delta)>Largest) {Axis=I;Largest=FMath::Abs(Delta);}
        }
        if (Axis!=INDEX_NONE)
        {
            CaptureInverted=Frame.Axes[Axis]<LearnStart.Axes[Axis]; Found(Axes[Axis]);return;
        }
    }
    for (FName Key:Before.Down)
        if (!Frame.Down.Contains(Key)) {Found(Key);return;}
}
void FFlightRadio::FeedFrame(const FRadioFrame& Next)
{
    FRadioFrame Previous=MoveTemp(Frame); Frame=Next;
    if (HaveFrame) Detect(Previous);
    else if (Learning()) LearnStart=Frame;
    HaveFrame=true;
    Evaluate(Stick,Frame.Down,false);
    bMoved |= FMath::Abs(Stick.Outputs[int32(A::Rudder)].Value)>.03f || FMath::Abs(Stick.Outputs[int32(A::Elevator)].Value)>.03f
        || FMath::Abs(Stick.Outputs[int32(A::Aileron)].Value)>.03f;
}
void FFlightRadio::Poll(APlayerController* PC,bool InMenu,bool ReserveWalkingKeys)
{
    bWalkingKeys=ReserveWalkingKeys && !InMenu;
    if (HasDevice())
    {
        FRadioFrame Next;
        if (Backend->Read(Next)) FeedFrame(Next);
        else {Backend->Close();Active=INDEX_NONE;Stick={};Frame={};HaveFrame=false;bMoved=false;CancelLearn();}
    }
    if (InMenu) Evaluate(Keys,MenuKeys,true);
    else
    {
        TSet<FName> Down;
        if (PC) for (const auto& B:Keys.Bindings)
            for (FName K:{B.Plus,B.Minus}) if (!K.IsNone() && PC->IsInputKeyDown(FKey(K))) Down.Add(K);
        Evaluate(Keys,Down,true);
    }
}
void FFlightRadio::Evaluate(FProfile& P,const TSet<FName>& Down,bool Keyboard)
{
    for (int32 I=0;I<ControlCount;++I)
    {
        auto& O=P.Outputs[I]; auto B=P.Bindings[I]; const T Type=ControlSpec(A(I)).Type;
        if(Keyboard && bWalkingKeys)
        {
            if(PilotMovement::IsWalkingKey(B.Plus)) B.Plus=NAME_None;
            if(PilotMovement::IsWalkingKey(B.Minus)) B.Minus=NAME_None;
        }
        const int32 Axis=Keyboard?INDEX_NONE:AxisIndex(B.Plus);
        const float Physical=Axis>=0?Frame.Axes[Axis]:0.f;
        const float V=B.Inverted?-Physical:Physical;
        bool Plus=Axis>=0 ? V>(O.WasPlus?.40f:.60f) : Down.Contains(B.Plus);
        const bool Minus=Down.Contains(B.Minus);
        if (Axis<0 && !B.Plus.IsNone() && B.Inverted && (Type==T::Pulse || Type==T::Switch)) Plus=!Plus;
        const bool Rise=Plus && !O.WasPlus && !O.Prime;
        const bool FallSide=Minus && !O.WasMinus && !O.Prime;
        const float Sign=B.Inverted?-1.f:1.f;
        const float Step=float(Rise)-float(FallSide);
        O.Pressed=Rise;
        switch (Type)
        {
        case T::Signed:
            O.Value=Axis>=0 ? Sign*StickCurve(Centered(Physical,Centers[Axis])) : Sign*(float(Plus)-float(Minus));
            break;
        case T::Trim:
            O.Memory=FMath::Clamp(O.Memory+Sign*Step*.02f,-.25f,.25f);
            O.Value=Axis>=0?Sign*Centered(Physical,Centers[Axis])*.25f:O.Memory;
            break;
        case T::CenteredRange:
            if (Axis>=0) O.Value=V;
            else if (!B.Minus.IsNone())
            {
                O.Memory=FMath::Clamp(O.Memory+Step*.2f,-1.f,1.f);
                O.Value=Sign*O.Memory;
            }
            else O.Value=Sign*float(Plus);
            break;
        case T::Range:
            if (Axis>=0) O.Value=.5f+.5f*V;
            else if (!B.Minus.IsNone()) {O.Memory=FMath::Clamp(O.Memory+Step*.1f,0.f,1.f);O.Value=B.Inverted?1.f-O.Memory:O.Memory;}
            else O.Value=B.Plus.IsNone()?0.f:float(B.Inverted?!Plus:Plus);
            break;
        case T::Switch:
            if (Rise && B.Toggle) O.Memory=O.Memory>.5f?0.f:1.f;
            O.Value=B.Toggle?O.Memory:float(Plus);
            break;
        case T::Pulse: O.Value=float(Plus);break;
        }
        O.WasPlus=Plus;O.WasMinus=Minus;O.Prime=false;
    }
}
float FFlightRadio::Preview(A Action) const {return Edited().Outputs[int32(Action)].Value;}
float FFlightRadio::Command(A Action) const
{
    const float K=Keys.Outputs[int32(Action)].Value, R=Stick.Outputs[int32(Action)].Value;
    const T Type=ControlSpec(Action).Type;
    if (Type==T::Range || Type==T::Switch || Type==T::Pulse) return FMath::Max(K,R);
    if (Type==T::CenteredRange) return HasDevice() && !Stick.Bindings[int32(Action)].Plus.IsNone()?R:K;
    const float Limit=Type==T::Trim?.25f:1.f;
    return FMath::Clamp(K+R,-Limit,Limit);
}
bool FFlightRadio::Pressed(A Action) const {return Keys.Outputs[int32(Action)].Pressed || Stick.Outputs[int32(Action)].Pressed;}
float FFlightRadio::SteeringCommand(bool UseAilerons) const
{
    const A Input = UseAilerons ? A::Aileron : A::Rudder;
    const A Trim = UseAilerons ? A::TrimAileron : A::TrimRudder;
    const float Rate = Command(A::LowRates) > .5f ? .7f : 1.f;
    return FMath::Clamp(Rate * Command(Input) + Command(Trim), -1.f, 1.f);
}
FString FFlightRadio::Shortcut(A Action) const
{
    const FName Key=Keys.Bindings[int32(Action)].Plus;
    const FName Source=Stick.Bindings[int32(Action)].Plus;
    if (HasDevice() && !Source.IsNone()) return InputLabel(Source);
    if (Key==TEXT("SpaceBar")) return TEXT("Espace");
    return Key.IsNone()?FString(TEXT("Libre")):FKey(Key).GetDisplayName().ToString();
}
