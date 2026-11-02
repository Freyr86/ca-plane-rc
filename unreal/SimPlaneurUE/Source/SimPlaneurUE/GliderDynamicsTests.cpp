#include "GliderDynamics.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGliderMotorTest,"SimPlaneur.Physics.MotorPower",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGliderMotorTest::RunTest(const FString&)
{
    FGliderDynamics Model;
    if (!TestTrue(TEXT("Load test aircraft"),Model.LoadModel(Model.TopyModelPath()))) return false;
    TestFalse(TEXT("Motor is opt-in"),Model.MotorSettings().Enabled);
    auto Speed = [&](bool Enabled,int32 Watts,double Throttle)
    {
        auto Flight = Model;
        Flight.SetMotor({Enabled,Watts});
        Flight.Reset(FVector::ZeroVector);
        for (int I=0; I<30; ++I) Flight.Step(0.,0.,0.,Throttle);
        return Flight.Airspeed();
    };
    const double Glide = Speed(false,100,0.);
    const double Half = Speed(true,100,.5);
    TestEqual(TEXT("Disabled motor ignores throttle"),Speed(false,100,1.),Glide);
    TestEqual(TEXT("Zero throttle glides"),Speed(true,100,0.),Glide);
    TestTrue(TEXT("Throttle increases speed"),Half>Glide && Speed(true,100,1.)>Half);
    TestEqual(TEXT("Watt limit scales delivered power"),Speed(true,50,1.),Half);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTopyReferenceTest, "SimPlaneur.Physics.TopyReference",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTopyReferenceTest::RunTest(const FString&)
{
    FGliderDynamics Flight;
    if (!TestTrue(TEXT("Load native Topy"), Flight.LoadModel(Flight.TopyModelPath()))) return false;
    FString Data;
    TSharedPtr<FJsonObject> Fixture;
    const FString Filename=FPaths::ProjectDir()/TEXT("SourceAssets/Tests/TopyFlightReference.json");
    if (!TestTrue(TEXT("Load JSBSim checkpoints"), FFileHelper::LoadFileToString(Data,*Filename) &&
        FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Data),Fixture) && Fixture.IsValid())) return false;
    TestEqual(TEXT("Physics frequency"),1./Flight.TimeStep,Fixture->GetNumberField(TEXT("hz")));
    for (const auto& Entry : Fixture->GetArrayField(TEXT("profiles")))
    {
        Flight.Reset();
        const auto& Profile=Entry->AsObject();
        const FString Name=Profile->GetStringField(TEXT("name"));
        const bool Loop=Name==TEXT("loop");
        const auto& Samples=Profile->GetArrayField(TEXT("samples"));
        int32 Tick=0, Index=0;
        double MaxDistance=0., MaxAngle=0., MaxSpeed=0., PeakBank=0.;
        double LastPitch=-1.4, UnwrappedPitch=LastPitch, PeakPitch=LastPitch;
        for (const auto& Segment : Profile->GetArrayField(TEXT("segments")))
        {
            const auto& Cmd=Segment->AsArray();
            for (int32 I=0; I<int32(Cmd[0]->AsNumber()); ++I)
            {
                Flight.Step(Cmd[1]->AsNumber(),Cmd[2]->AsNumber());
                ++Tick;
                const FTransform Pose=Flight.WorldPose();
                if (Pose.ContainsNaN() || !FMath::IsFinite(Flight.Airspeed())) { AddError(Name+TEXT(" diverged")); return false; }
                const FVector Nose=Pose.GetRotation().GetForwardVector();
                const double Pitch=FMath::RadiansToDegrees(FMath::Atan2(Nose.Z,Nose.X));
                UnwrappedPitch+=FMath::FindDeltaAngleDegrees(LastPitch,Pitch);
                PeakPitch=FMath::Max(PeakPitch,UnwrappedPitch);
                PeakBank=FMath::Max(PeakBank,FMath::Abs(Pose.Rotator().Roll));
                LastPitch=Pitch;
                if (!Samples.IsValidIndex(Index)) continue;
                const auto& Sample=Samples[Index]->AsArray();
                if (Tick!=int32(Sample[0]->AsNumber())) continue;
                const FVector Reference(Sample[1]->AsNumber(),Sample[2]->AsNumber(),Sample[3]->AsNumber());
                const FRotator Attitude(Sample[4]->AsNumber(),Sample[5]->AsNumber(),Sample[6]->AsNumber());
                MaxDistance=FMath::Max(MaxDistance,FVector::Dist(Pose.GetLocation()*.01,Reference));
                MaxAngle=FMath::Max(MaxAngle,FMath::RadiansToDegrees(Pose.GetRotation().AngularDistance(Attitude.Quaternion())));
                MaxSpeed=FMath::Max(MaxSpeed,FMath::Abs(Flight.Airspeed()-Sample[7]->AsNumber()));
                ++Index;
            }
        }
        UE_LOG(LogTemp,Display,TEXT("NATIVE_COMPARE %s max_xyz_m=%.3f max_att_deg=%.3f max_speed_mps=%.3f end_speed=%.3f end_alt=%.3f end_sink=%.3f peak_bank=%.2f peak_pitch_unwrapped=%.2f"),
            *Name,MaxDistance,MaxAngle,MaxSpeed,Flight.Airspeed(),Flight.Altitude(),Flight.SinkRate(),PeakBank,PeakPitch);
        TestEqual(Name+TEXT(" all checkpoints"), Index, Samples.Num());
        TestTrue(Name+TEXT(" position within 0.5 m"), MaxDistance<.5);
        TestTrue(Name+TEXT(" attitude within 2 degrees"), MaxAngle<2.);
        TestTrue(Name+TEXT(" airspeed within 0.12 m/s"), MaxSpeed<.12);
        if (Loop) TestTrue(TEXT("Complete a full loop"),PeakPitch>360.);
        if (Name==TEXT("turn")) TestTrue(TEXT("Banked turn"),PeakBank>15.);
    }
    const FTransform Contact(FRotator(15,30,-45),FVector(1200,-200,-350));
    Flight.StopAt(Contact);
    Flight.Step(1,1);
    TestTrue(TEXT("Contact holds the resolved pose below lake datum"),Flight.WorldPose().Equals(Contact,1.e-6));
    TestEqual(TEXT("Contact zero speed"),Flight.Airspeed(),0.);
    Flight.Reset();
    TestFalse(TEXT("Reset releases contact"), Flight.IsStopped());
    TestEqual(TEXT("Launch altitude"),Flight.Altitude(),80.);
    return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTopyLoadingTest, "SimPlaneur.Physics.TopyLoading",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTopyLoadingTest::RunTest(const FString&)
{
    FGliderDynamics Original;
    if (!TestTrue(TEXT("Load Topy"), Original.LoadModel(Original.TopyModelPath()))) return false;
    const auto Standard = Original.StandardLoading();
    auto Fly = [&](FGliderLoading Setting)
    {
        FGliderDynamics Model = Original;
        Model.SetLoading(Setting);
        Model.Reset();
        for (int32 I = 0; I < 60; ++I) Model.Step(0,0);
        return Model;
    };
    const auto Nominal = Fly(Standard);
    auto Forward = Fly({Standard.MassGrams, Standard.CgMm - 4});
    auto Aft = Fly({Standard.MassGrams, Standard.CgMm + 4});
    const auto Heavy = Fly({Standard.MassGrams + 40, Standard.CgMm});
    TestTrue(TEXT("Forward CG pitches below nominal"), Forward.WorldPose().Rotator().Pitch < Nominal.WorldPose().Rotator().Pitch);
    TestTrue(TEXT("Aft CG pitches above nominal"), Aft.WorldPose().Rotator().Pitch > Nominal.WorldPose().Rotator().Pitch);
    TestTrue(TEXT("Larger mass reduces initial climb"), Heavy.Altitude() < Nominal.Altitude());
    Aft.SetLoading({1000000, -100});
    TestTrue(TEXT("Limits applied"), Aft.Loading() == Original.LimitLoading({1000000, -100}));
    Aft.SetLoading(Standard);
    Aft.Reset();
    for (int32 I = 0; I < 60; ++I) Aft.Step(0,0);
    TestTrue(TEXT("Restoring the original setup restores trajectory"), Aft.WorldPose().Equals(Nominal.WorldPose(), 1.e-8));
    const FTransform Contact(FRotator(13,21,7),FVector(1100,150,-300));
    Forward.StopAt(Contact);
    TestTrue(TEXT("Shifted CG preserves contact pose"), Forward.WorldPose().Equals(Contact,1.e-7));
    return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FK3600FlightTest, "SimPlaneur.Physics.K3600Flight",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FK3600FlightTest::RunTest(const FString&)
{
    FGliderDynamics Straight;
    if (!TestTrue(TEXT("Load K3600"), Straight.LoadModel(FPaths::ProjectContentDir()/TEXT("FlightModels/K3600.json")))) return false;
    FGliderDynamics Turn = Straight, Pull = Straight;
    for (int32 I=0; I<600; ++I)
    {
        Straight.Step(0,0);
        Turn.Step(0,I>360 ? -.35 : 0.);
        Pull.Step(I>480 ? -.35 : 0.,0);
    }
    TestTrue(TEXT("Finite flying airspeed"), Straight.Airspeed()>4. && Straight.Airspeed()<16.);
    TestTrue(TEXT("Direction banks the aircraft"), FMath::Abs(Turn.WorldPose().Rotator().Roll)>3.);
    const double HeadingChange = FMath::Abs(FMath::FindDeltaAngleDegrees(Straight.WorldPose().Rotator().Yaw,Turn.WorldPose().Rotator().Yaw));
    TestTrue(TEXT("Rudder changes the heading"),HeadingChange>2.);
    TestTrue(TEXT("Elevator raises the nose"), Pull.WorldPose().Rotator().Pitch>Straight.WorldPose().Rotator().Pitch+1.);
    UE_LOG(LogTemp,Display,TEXT("K3600_RESPONSE bank_deg=%.2f heading_deg=%.2f pitch_gain_deg=%.2f glide_speed=%.2f"),
        FMath::Abs(Turn.WorldPose().Rotator().Roll),HeadingChange,Pull.WorldPose().Rotator().Pitch-Straight.WorldPose().Rotator().Pitch,Straight.Airspeed());
    FGliderDynamics Dive = Straight;
    for (int32 I=0; I<240; ++I) Dive.Step(.35,0);
    TestTrue(TEXT("Dive builds airspeed"),Dive.Airspeed()>Straight.Airspeed());
    FGliderDynamics FastTurn = Dive, Recovery = Dive;
    const double DiveSpeed = Dive.Airspeed();
    for (int32 I=0; I<60; ++I)
    {
        Dive.Step(0,0);
        FastTurn.Step(0,-.35);
        Recovery.Step(-.35,0);
    }
    const double FastBank = FMath::Abs(FMath::FindDeltaAngleDegrees(Dive.WorldPose().Rotator().Roll,FastTurn.WorldPose().Rotator().Roll));
    const double RecoveryPitch = FMath::FindDeltaAngleDegrees(Dive.WorldPose().Rotator().Pitch,Recovery.WorldPose().Rotator().Pitch);
    const double FastHeading = FMath::Abs(FMath::FindDeltaAngleDegrees(Dive.WorldPose().Rotator().Yaw,FastTurn.WorldPose().Rotator().Yaw));
    TestTrue(TEXT("Rudder yaws the aircraft within half a second after diving"),FastHeading>1.);
    TestTrue(TEXT("Steering responds within half a second after diving"),FastBank>2.);
    TestTrue(TEXT("Elevator responds within half a second after diving"),RecoveryPitch>2.);
    UE_LOG(LogTemp,Display,TEXT("K3600_DIVE_RESPONSE speed=%.2f bank_half_second=%.2f heading_half_second=%.2f pitch_half_second=%.2f"),DiveSpeed,FastBank,FastHeading,RecoveryPitch);
    for (int32 Cg : {94,104,114})
    {
        FGliderDynamics Model = Straight;
        Model.SetLoading({1600,Cg});
        Model.Reset();
        for (int32 I=0; I<1200; ++I)
        {
            const double Command = I<360 ? 0. : I<480 ? -1. : I<600 ? 1. : 0.;
            Model.Step(Command,Command);
            if (Model.WorldPose().ContainsNaN() || !FMath::IsFinite(Model.Airspeed()))
            {
                AddError(FString::Printf(TEXT("K3600 full-stick response diverged at CG %d"),Cg));
                return false;
            }
        }
    }
    return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FControlGainsTest, "SimPlaneur.Physics.ControlGains",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FControlGainsTest::RunTest(const FString&)
{
    for (const TCHAR* Name : {TEXT("Topy4522"),TEXT("K3600")})
    {
        FGliderDynamics Original;
        if (!TestTrue(TEXT("Load model for control gains"),Original.LoadModel(FPaths::ProjectContentDir()/TEXT("FlightModels")/(FString(Name)+TEXT(".json"))))) return false;
        TestTrue(TEXT("Existing models default to 100 percent"),Original.ControlGains()==FGliderControlGains{});
        auto Probe = [&](FGliderControlGains Gains,double Pitch,double Yaw)
        {
            FGliderDynamics Model=Original;
            Model.SetControlGains(Gains);
            Model.Reset();
            for (int32 I=0; I<24; ++I) Model.Step(Pitch,Yaw);
            return Model.WorldPose();
        };
        const FTransform Neutral=Probe({100,100,100},0,0);
        TestTrue(TEXT("Zero effectiveness cancels control forces"),Probe({0,0,0},-.25,-.25).Equals(Neutral,1.e-7));
        for (int32 Axis=0; Axis<3; ++Axis)
        {
            auto Response = [&](int32 Gain)
            {
                FGliderControlGains Setting{0,0,0};
                int32* Values[]={&Setting.Roll,&Setting.Pitch,&Setting.Yaw};
                *Values[Axis]=Gain;
                const FRotator Pose=Probe(Setting,-.25,-.25).Rotator(), Baseline=Neutral.Rotator();
                const double Angles[]={Pose.Roll,Pose.Pitch,Pose.Yaw}, BaseAngles[]={Baseline.Roll,Baseline.Pitch,Baseline.Yaw};
                return FMath::FindDeltaAngleDegrees(BaseAngles[Axis],Angles[Axis]);
            };
            const double Normal=Response(100);
            TestTrue(TEXT("500 percent increases authority beyond 100 percent"),FMath::Abs(Response(500))>1.2*FMath::Abs(Normal));
            TestTrue(TEXT("Negative gain reverses the control response"),Normal*Response(-100)<0.);
        }
        Original.SetControlGains({501,-501,1500});
        TestTrue(TEXT("Backend clamps settings to supported limits"),Original.ControlGains()==FGliderControlGains{500,-500,500});
        Original.Reset();
        TestTrue(TEXT("Relaunch retains gains"),Original.ControlGains()==FGliderControlGains{500,-500,500});
        for (int32 Mask=0; Mask<8; ++Mask)
        {
            FGliderDynamics Model=Original;
            Model.SetControlGains({(Mask&1)?500:-500,(Mask&2)?500:-500,(Mask&4)?500:-500});
            Model.Reset();
            for (int32 I=0; I<1200; ++I)
            {
                const double Command=I<360 ? -1. : I<720 ? 1. : 0.;
                Model.Step(Command,Command);
                if (Model.WorldPose().ContainsNaN() || !FMath::IsFinite(Model.Airspeed()))
                {
                    AddError(FString::Printf(TEXT("%s diverged at extreme gain combination %d"),Name,Mask));
                    return false;
                }
            }
        }
    }
    return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHandLaunchTest, "SimPlaneur.Physics.HandLaunch",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHandLaunchTest::RunTest(const FString&)
{
    for (const TCHAR* Name : {TEXT("Topy4522"),TEXT("K3600"),TEXT("VJX2022")})
    {
        FGliderDynamics Model;
        if (!TestTrue(TEXT("Load hand-launch model"),Model.LoadModel(FPaths::ProjectContentDir()/TEXT("FlightModels")/(FString(Name)+TEXT(".json"))))) return false;
        Model.SetLoading({Model.StandardLoading().MassGrams,Model.StandardLoading().CgMm+5});
        Model.SetControlGains({120,100,140});
        const FTransform Held(FRotator(6,25,0),FVector(6400,-1755,8000));
        Model.StopAt(Held);
        for (int32 I=0;I<120;++I) Model.Step(1,1);
        TestTrue(TEXT("Held aircraft cannot fall or respond to sticks"),Model.WorldPose().Equals(Held,1.e-7));
        const FVector Velocity=FRotator(0,25,0).RotateVector(FVector(8,0,.8));
        Model.LaunchFrom(Held,Velocity);
        TestFalse(TEXT("Release enables dynamics"),Model.IsStopped());
        TestTrue(TEXT("Release preserves exact visual position with adjusted CG"),Model.WorldPose().Equals(Held,1.e-7));
        TestTrue(TEXT("Launch velocity is ground-relative and converted to NED"),
            (Model.LocalWind()-Model.RelativeFlowNED()).Equals(FVector(Velocity.X,Velocity.Y,-Velocity.Z),1.e-7));
        TestTrue(TEXT("Release retains tuned control gains"),Model.ControlGains()==FGliderControlGains{120,100,140});
        for (int32 I=0;I<120;++I) Model.Step(0,0);
        TestTrue(TEXT("Throw transitions into forward flight"),FVector::DotProduct(Model.WorldPose().GetLocation()-Held.GetLocation(),Velocity.GetSafeNormal())>500.);
        TestFalse(TEXT("Hand launch stays finite"),Model.WorldPose().ContainsNaN());
    }
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVjxAileronTest, "SimPlaneur.Physics.VjxAilerons",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVjxAileronTest::RunTest(const FString&)
{
    FGliderDynamics Right;
    if (!TestTrue(TEXT("Load VJX"),Right.LoadModel(FPaths::ProjectContentDir()/TEXT("FlightModels/VJX2022.json")))) return false;
    Right.Reset(FVector::ZeroVector);
    Right.LaunchFrom(FTransform(FQuat::Identity,FVector(0,0,8000)),FVector(15,0,0));
    FGliderDynamics Left = Right;
    for (int32 I=0; I<30; ++I)
    {
        Right.Step(0,0,1);
        Left.Step(0,0,-1);
    }
    const double BankR = Right.WorldPose().TransformVectorNoScale(FVector::RightVector).Z;
    const double BankL = Left.WorldPose().TransformVectorNoScale(FVector::RightVector).Z;
    TestTrue(TEXT("Ailerons alone lower the commanded wing"), BankR < -.05 && BankL > .05);
    return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGliderFlapsTest, "SimPlaneur.Physics.Flaps",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGliderFlapsTest::RunTest(const FString&)
{
    for (const TCHAR* Name : {TEXT("Topy4522"),TEXT("K3600"),TEXT("VJX2022")})
    {
        FGliderDynamics Clean;
        if (!TestTrue(Name,Clean.LoadModel(FPaths::ProjectContentDir()/TEXT("FlightModels")/(FString(Name)+TEXT(".json"))))) return false;
        Clean.Reset(FVector::ZeroVector);
        FGliderDynamics Extended = Clean;
        Extended.AdvanceFlaps(1.,.1);
        if (FString(Name)==TEXT("Topy4522"))
        {
            TestFalse(TEXT("Two-axis Topy has fixed trailing edge"),Extended.HasFlaps());
            TestEqual(TEXT("No flap movement"),Extended.FlapAngleDeg(),0.);
            continue;
        }
        TestEqual(TEXT("Progressive servo"),Extended.FlapAngleDeg(),9.);
        Extended.SetFlapTravel({12,45});
        Extended.AdvanceFlaps(-1.,1.);
        TestEqual(TEXT("Upper endpoint"),Extended.FlapAngleDeg(),-12.);
        Extended.AdvanceFlaps(-.5,1.);
        TestEqual(TEXT("Half negative travel"),Extended.FlapAngleDeg(),-6.);
        Extended.AdvanceFlaps(0.,1.);
        TestEqual(TEXT("Mid-stick is neutral"),Extended.FlapAngleDeg(),0.);
        Extended.AdvanceFlaps(1.,1.);
        TestEqual(TEXT("Lower endpoint"),Extended.FlapAngleDeg(),45.);
        Extended.AdvanceFlaps(-.5,1.,1.);
        TestEqual(TEXT("Crow overrides reflex at full deployment"),Extended.FlapAngleDeg(),45.);
        TestEqual(TEXT("Outer wings at full crow"),Extended.CrowFraction(),1.);
        Extended.AdvanceFlaps(-.5,1.,0.);
        TestEqual(TEXT("Retracting crow restores manual reflex"),Extended.FlapAngleDeg(),-6.);
        Extended.AdvanceFlaps(.35,1.);
        const FTransform Pose(FQuat::Identity,FVector(0,0,8000));
        for (auto* Model : {&Clean,&Extended})
        {
            Model->LaunchFrom(Pose,FVector(14,0,0));
            Model->Step(0,0);
        }
        TestTrue(TEXT("Flapped wing sinks less at equal initial speed"),Extended.SinkRate()<Clean.SinkRate());
        TestTrue(TEXT("Flaps increase drag"),Extended.Airspeed()<Clean.Airspeed());
        Extended.AdvanceFlaps(0.,1.);
        TestEqual(TEXT("Fully retracts"),Extended.FlapAngleDeg(),0.);
    }
    return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVjxInvertedGlideTest,"SimPlaneur.Physics.VjxInvertedGlide",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVjxInvertedGlideTest::RunTest(const FString&)
{
    FGliderDynamics Model;
    if (!TestTrue(TEXT("Load VJX"),Model.LoadModel(FPaths::ProjectContentDir()/TEXT("FlightModels/VJX2022.json")))) return false;
    Model.Reset(FVector::ZeroVector);
    Model.LaunchFrom(FTransform(FRotator(-4.5,0.,180.),FVector(0,0,10000)),FVector(16.,0.,-3.));
    for (int32 I=0; I<720; ++I)
    {
        Model.Step(.62,0.,0.);
        if (Model.WorldPose().ContainsNaN() || Model.WorldPose().GetRotation().GetUpVector().Z>-.90)
            return TestTrue(TEXT("Stays inverted throughout"),false);
    }
    UE_LOG(LogTemp,Display,TEXT("VJX_INVERTED loss_m=%.2f speed=%.2f sink=%.2f"),100.-Model.Altitude(),Model.Airspeed(),Model.SinkRate());
    TestTrue(TEXT("Unpowered inverted glide at moderate forward stick"),Model.Altitude()>76. && Model.Altitude()<100.);
    TestTrue(TEXT("No runaway dive"),Model.Airspeed()>12. && Model.Airspeed()<20.);
    return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFoxFlightTest,"SimPlaneur.Physics.Fox7m",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFoxFlightTest::RunTest(const FString&)
{
    FGliderDynamics Clean;
    if (!TestTrue(TEXT("Load Fox"),Clean.LoadModel(FPaths::ProjectContentDir()/TEXT("FlightModels/Fox7m.json")))) return false;
    TestEqual(TEXT("Scale flight mass"),Clean.StandardLoading().MassGrams,24000);
    TestTrue(TEXT("Independent spoilers, clean wing"),Clean.HasSpoilers() && !Clean.HasFlaps());
    Clean.Reset(FVector::ZeroVector);
    Clean.LaunchFrom(FTransform(FRotator(1.,0.,0.),FVector(0,0,6000)),FVector(22.,0.,0.));
    FGliderDynamics Braked=Clean, Right=Clean, Inverted=Clean;
    Inverted.LaunchFrom(FTransform(FRotator(-5.,0.,180.),FVector(0,0,6000)),FVector(22.,0.,-1.));
    for (int32 I=0; I<720; ++I)
    {
        Clean.Step(0,0,0);
        Braked.AdvanceSpoilers(1.,FGliderDynamics::TimeStep);
        Braked.Step(0,0,0);
        Inverted.Step(.46,0,0);
        if (I<60) Right.Step(0,0,1.);
        if (Inverted.WorldPose().ContainsNaN() || Inverted.WorldPose().GetRotation().GetUpVector().Z>-.90)
            return TestTrue(TEXT("Controlled inverted attitude throughout"),false);
    }
    TestTrue(TEXT("Air start transitions to free glide"),Clean.Altitude()>48 && Clean.Altitude()<65);
    TestTrue(TEXT("Ailerons bank the Fox"),FMath::Abs(Right.WorldPose().Rotator().Roll)>15.);
    TestTrue(TEXT("Spoilers increase altitude loss"),Braked.Altitude()<Clean.Altitude()-2.);
    TestTrue(TEXT("Inverted glide without runaway dive"),Inverted.Altitude()>45. && Inverted.Altitude()<60. && Inverted.Airspeed()>16. && Inverted.Airspeed()<28.);
    UE_LOG(LogTemp,Display,TEXT("FOX_FLIGHT clean_h=%.2f braked_h=%.2f inverted_h=%.2f inverted_V=%.2f bank=%.1f"),
        Clean.Altitude(),Braked.Altitude(),Inverted.Altitude(),Inverted.Airspeed(),Right.WorldPose().Rotator().Roll);
    return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJS3GlideTest,"SimPlaneur.Physics.JS3Glide",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FJS3GlideTest::RunTest(const FString&)
{
    FGliderDynamics Clean;
    if (!TestTrue(TEXT("Load JS-3"),Clean.LoadModel(FPaths::ProjectContentDir()/TEXT("FlightModels/JS3_9m.json")))) return false;
    TestTrue(TEXT("Flaps and separate airbrakes"),Clean.HasFlaps() && Clean.HasSpoilers());
    Clean.Reset(FVector::ZeroVector);
    Clean.LaunchFrom(FTransform(FRotator(6.,0.,0.),FVector(0,0,157)),FVector(21.,0.,0.));
    for (int32 I=0; I<120; ++I)
    {
        Clean.Step(0,0,0);
        if (Clean.Altitude()<.5 || Clean.WorldPose().ContainsNaN())
            return TestTrue(TEXT("Clears level ground after release"),false);
    }
    FGliderDynamics Braked=Clean, Banked=Clean;
    for (int32 I=0; I<600; ++I)
    {
        Clean.Step(0,0,0);
        Braked.AdvanceSpoilers(1.,FGliderDynamics::TimeStep);
        Braked.Step(0,0,0);
        if (I<120) Banked.Step(0,0,.7);
    }
    TestTrue(TEXT("Clean glide stays in flying speed range"),Clean.Airspeed()>13. && Clean.Airspeed()<30.);
    TestTrue(TEXT("Airbrakes steepen the glide"),Braked.Altitude()<Clean.Altitude()-2.);
    TestTrue(TEXT("Independent ailerons respond"),FMath::Abs(Banked.WorldPose().Rotator().Roll)>10.);
    return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscusClimbTest,"SimPlaneur.Physics.DiscusClimb",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscusClimbTest::RunTest(const FString&)
{
    FGliderDynamics Model;
    if (!TestTrue(TEXT("Load Snipe 2"),Model.LoadModel(FPaths::ProjectContentDir()/TEXT("FlightModels/Snipe2.json")))) return false;
    TestTrue(TEXT("Full-span flaperons"),Model.HasFlaps() && !Model.HasSpoilers());
    Model.Reset(FVector::ZeroVector);
    const FRotator Attitude(55.,0.,0.);
    Model.LaunchFrom(FTransform(Attitude,FVector(0.,144.,145.)),Attitude.Vector()*30.16);
    double Peak=Model.Altitude();
    for (int32 I=0; I<1200; ++I)
    {
        Model.Step(0.,0.,0.);
        Peak=FMath::Max(Peak,Model.Altitude());
        if (Model.WorldPose().ContainsNaN() || Model.Altitude()<1.)
            return TestTrue(TEXT("Stays airborne throughout the climb and transition"),false);
    }
    UE_LOG(LogTemp,Display,TEXT("DLG_CLIMB calm_peak_m=%.2f altitude_10s_m=%.2f airspeed_10s_mps=%.2f"),Peak,Model.Altitude(),Model.Airspeed());
    TestTrue(TEXT("Converts release energy into a climb"),Peak>20. && Peak<48.);
    TestTrue(TEXT("Returns to gliding flight"),Model.Altitude()<Peak && Model.Altitude()>Peak-7.
        && Model.Airspeed()>4. && Model.Airspeed()<11. && FMath::Abs(Model.SinkRate())<2.);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLongBowTest,"SimPlaneur.Physics.LongBowElevons",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLongBowTest::RunTest(const FString&)
{
    FGliderDynamics Glide;
    if (!TestTrue(TEXT("Load LongBow"),Glide.LoadModel(FPaths::ProjectContentDir()/TEXT("FlightModels/LongBowMk2.json")))) return false;
    TestTrue(TEXT("Published loading"),Glide.Loading()==FGliderLoading{1074,214});
    TestTrue(TEXT("Two elevons and pusher"),Glide.HasElevons() && !Glide.HasFlaps() && Glide.MotorSettings().Enabled);
    Glide.Reset(FVector::ZeroVector);
    Glide.LaunchFrom(FTransform(FRotator::ZeroRotator,FVector(0,0,8000)),FVector(10.,0.,0.));
    auto Roll=Glide, Pitch=Glide, Yaw=Glide, Power=Glide;
    for (int32 I=0; I<48; ++I)
    {
        Roll.Step(0,0,.6);
        Pitch.Step(-.6,0,0.);
        Yaw.Step(0,.8,0.);
        Power.Step(0,0,0.,.6);
        Glide.Step(0,0,0.);
    }
    TestTrue(TEXT("Elevons bank the wing"),FMath::Abs(Roll.WorldPose().Rotator().Roll)>4.);
    TestTrue(TEXT("Symmetric elevons pitch up"),Pitch.WorldPose().Rotator().Pitch>Glide.WorldPose().Rotator().Pitch+2.);
    TestTrue(TEXT("No separate rudder"),Yaw.WorldPose().Equals(Glide.WorldPose(),1.e-5));
    TestTrue(TEXT("Pusher accelerates"),Power.Airspeed()>Glide.Airspeed()+.4);
    for (int32 I=0; I<1200; ++I) Glide.Step(0,0,0.);
    UE_LOG(LogTemp,Display,TEXT("LONGBOW_GLIDE airspeed_mps=%.2f altitude_m=%.2f sink_mps=%.2f"),Glide.Airspeed(),Glide.Altitude(),Glide.SinkRate());
    TestTrue(TEXT("Sustained neutral glide"),!Glide.WorldPose().ContainsNaN() && Glide.Altitude()>55.
        && Glide.Airspeed()>5. && Glide.Airspeed()<20. && FMath::Abs(Glide.SinkRate())<3.);
    return !HasAnyErrors();
}
#endif
