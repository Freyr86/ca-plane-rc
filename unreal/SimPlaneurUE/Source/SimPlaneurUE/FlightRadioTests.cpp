#if WITH_DEV_AUTOMATION_TESTS
#include "FlightRadio.h"
#include "Misc/AutomationTest.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGliderBindingsTest,"SimPlaneur.Controls.Bindings",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)

bool FGliderBindingsTest::RunTest(const FString&)
{
    using A=EGliderControl;
    FFlightRadio R;
    R.SettingsIni=FPaths::ConvertRelativePathToFull(FPaths::CreateTempFilename(*FPaths::ProjectIntermediateDir(),TEXT("RCAssign"),TEXT(".ini")));
    FConfigFile Empty;
    Empty.bCanSaveAllSections=true;
    GConfig->SetFile(R.SettingsIni,&Empty);
    FRadioDevice D;D.Key=TEXT("proof");D.LegacyKey=TEXT("old");D.Axes={0,1,2,3,4,5,6,7};D.Buttons=24;D.Hats=1;
    R.Devices.Add(D);R.Active=0;
    GConfig->SetInt(TEXT("SimPlaneur.Radio.old"),TEXT("Axis0"),4,R.SettingsIni);
    GConfig->SetFloat(TEXT("SimPlaneur.Radio.old"),TEXT("Center4"),.1f,R.SettingsIni);
    R.LoadProfile(R.Keys,true);R.LoadProfile(R.Stick,false);
    TestEqual(TEXT("WinMM U migrates to Rx"),R.Binding(A::Rudder).Plus,FName(TEXT("AxisRx")));
    TestEqual(TEXT("Legacy centre"),R.Centers[3],.1f);
    FRadioFrame F;
    F.Axes[3]=.1f;R.FeedFrame(F);
    R.BeginLearn(A::Rudder,false); F.Axes[7]=.12f;R.FeedFrame(F);
    TestTrue(TEXT("Ignore stationary jitter"),R.Learning());
    F.Axes[7]=-.9f;R.FeedFrame(F);
    TestTrue(TEXT("Eighth axis and positive direction learned"),R.Binding(A::Rudder).Inverted && R.Command(A::Rudder)>.8f);
    R.BeginLearn(A::Elevator,false); F.Axes[7]=.7f;R.FeedFrame(F);
    TestTrue(TEXT("Duplicate stages a decision"),R.HasConflict());
    R.CancelLearn();TestEqual(TEXT("Cancel keeps existing elevator"),R.Binding(A::Elevator).Plus,FName(TEXT("AxisY")));
    R.BeginLearn(A::Aileron,false);F.Axes[7]=-.6f;R.FeedFrame(F);R.Resolve(false);
    TestEqual(TEXT("Explicit sharing"),R.Binding(A::Aileron).Plus,R.Binding(A::Rudder).Plus);
    R.BeginLearn(A::Aux1,false);F.Axes[7]=.9f;R.FeedFrame(F);R.Resolve(true);
    TestTrue(TEXT("Move clears previous owners"),R.Binding(A::Rudder).Plus.IsNone() && R.Binding(A::Aileron).Plus.IsNone());
    R.BeginLearn(A::Camera,false);F.Down.Add(TEXT("B24"));R.FeedFrame(F);
    TestFalse(TEXT("Binding press never fires in flight"),R.Pressed(A::Camera));
    F.Down.Reset();R.FeedFrame(F);F.Down.Add(TEXT("B24"));R.FeedFrame(F);
    TestTrue(TEXT("Mapped button 24 rises"),R.Pressed(A::Camera));
    R.FeedFrame(F);TestFalse(TEXT("Held button does not repeat"),R.Pressed(A::Camera));
    R.BeginLearn(A::TrimRudder,false);F.Down.Add(TEXT("Hat1.2"));R.FeedFrame(F);
    F.Down.Remove(TEXT("Hat1.2"));R.FeedFrame(F); F.Down.Add(TEXT("Hat1.2"));R.FeedFrame(F);
    TestEqual(TEXT("POV trim step"),R.Command(A::TrimRudder),.02f);
    R.FeedFrame(F);TestEqual(TEXT("No trim drift while held"),R.Command(A::TrimRudder),.02f);
    R.BeginLearn(A::Spoilers,false);F.Axes[6]=1.f;R.FeedFrame(F);
    TestEqual(TEXT("Spoiler fully open"),R.Command(A::Spoilers),1.f);
    F.Axes[6]=-1.f;R.FeedFrame(F);TestEqual(TEXT("Spoiler fully closed"),R.Command(A::Spoilers),0.f);
    R.BeginLearn(A::Butterfly,false);F.Axes[2]=-1.f;R.FeedFrame(F);
    TestEqual(TEXT("Full crow"),R.Command(A::Butterfly),1.f);
    F.Axes[2]=0.f;R.FeedFrame(F);TestEqual(TEXT("Half crow"),R.Command(A::Butterfly),.5f);
    F.Axes[2]=1.f;R.FeedFrame(F);TestEqual(TEXT("Crow retracted"),R.Command(A::Butterfly),0.f);
    R.BeginLearn(A::LowRates,false);F.Down.Add(TEXT("B5"));R.FeedFrame(F);R.ToggleMode(A::LowRates);
    F.Down.Remove(TEXT("B5"));R.FeedFrame(F);F.Down.Add(TEXT("B5"));R.FeedFrame(F);
    F.Down.Remove(TEXT("B5"));R.FeedFrame(F);TestEqual(TEXT("Latching rates stay on"),R.Command(A::LowRates),1.f);
    GConfig->UnloadFile(R.SettingsIni);
    GConfig->LoadFile(R.SettingsIni);
    R.LoadProfile(R.Stick,false);
    TestEqual(TEXT("Physical binding saved"),R.Binding(A::Camera).Plus,FName(TEXT("B24")));
    TestTrue(TEXT("Cleared migration stays cleared after reload"),R.Binding(A::Rudder).Plus.IsNone());
    R.Active=INDEX_NONE;R.LoadProfile(R.Stick,false);R.Preferred=TEXT("Keyboard");
    R.BeginLearn(A::Elevator,true);R.MenuKey(TEXT("F9"),true);R.Evaluate(R.Keys,R.MenuKeys,true);
    TestEqual(TEXT("Negative button assigned"),R.Command(A::Elevator),-1.f);
    R.MenuKey(TEXT("F9"),false);R.Evaluate(R.Keys,R.MenuKeys,true);
    TestEqual(TEXT("Key release"),R.Command(A::Elevator),0.f);
    R.Clear(A::Elevator,true);R.LoadProfile(R.Keys,true);
    TestTrue(TEXT("Keyboard clear persists"),R.Binding(A::Elevator).Minus.IsNone());

    // Distinct sticks must remain independent when a two-axis model uses ailerons for steering.
    R.Keys={}; R.Stick={}; R.Active=0; R.Preferred=TEXT("Auto"); R.bMoved=false;
    R.Stick.Bindings[int32(A::Rudder)].Plus=TEXT("AxisRx");
    R.Stick.Bindings[int32(A::Aileron)].Plus=TEXT("AxisX");
    F={}; F.Axes[0]=.8f; F.Axes[3]=R.Centers[3]; R.FeedFrame(F);
    TestTrue(TEXT("Aileron-only movement activates radio"),R.IsDriving());
    TestTrue(TEXT("Aileron stick steers when selected"),R.SteeringCommand(true)>.6f);
    TestEqual(TEXT("Other stick stays neutral"),R.SteeringCommand(false),0.f);
    F.Axes[3]=-.5f; R.FeedFrame(F);
    TestTrue(TEXT("Opposite rudder input stays independent"),R.SteeringCommand(false)<-.4f && R.SteeringCommand(true)>.6f);
    const float Untrimmed=R.SteeringCommand(true);
    const float RudderOnly=R.SteeringCommand(false);
    R.Stick.Bindings[int32(A::TrimAileron)].Plus=TEXT("Slider2");
    F.Axes[7]=-.4f; R.FeedFrame(F);
    TestTrue(TEXT("Selected stick carries its trim"),FMath::IsNearlyEqual(R.SteeringCommand(true),Untrimmed-.1f));
    TestEqual(TEXT("Aileron trim leaves rudder unchanged"),R.SteeringCommand(false),RudderOnly);
    R.Stick.Bindings[int32(A::LowRates)].Plus=TEXT("B5");
    F.Down.Add(TEXT("B5")); R.FeedFrame(F);
    TestTrue(TEXT("Rates affect stick but preserve trim"),FMath::IsNearlyEqual(R.SteeringCommand(true),Untrimmed*.7f-.1f));
    F.Down.Reset(); R.Stick.Bindings[int32(A::Aileron)].Inverted=true; R.FeedFrame(F);
    TestTrue(TEXT("Selected axis inversion is respected"),R.SteeringCommand(true)<-.6f);
    TestEqual(TEXT("Unassigned flaps rest at neutral"),R.Command(A::Flaps),0.f);
    R.Stick.Bindings[int32(A::Flaps)].Plus=TEXT("AxisX");
    for (float Travel : {-1.f,-.5f,0.f,.5f,1.f})
    {
        F.Axes[0]=Travel;
        R.FeedFrame(F);
        TestEqual(TEXT("Both halves of the flap axis reach flight controls"),R.Command(A::Flaps),Travel);
    }
    R.Active=INDEX_NONE; R.Stick={}; R.Preferred=TEXT("Keyboard"); R.LoadProfile(R.Keys,true);
    R.Evaluate(R.Keys,{FName(TEXT("Right"))},true);
    TestEqual(TEXT("Keyboard right arrow steers through aileron input"),R.SteeringCommand(true),1.f);
    GConfig->UnloadFile(R.SettingsIni);
    IFileManager::Get().Delete(*R.SettingsIni);
    return true;
}
#endif
