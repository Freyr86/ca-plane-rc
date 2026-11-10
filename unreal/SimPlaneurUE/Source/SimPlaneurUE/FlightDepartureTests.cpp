#include "GliderDynamics.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFlightDepartureTest,"SimPlaneur.Physics.FlightDeparture",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFlightDepartureTest::RunTest(const FString&)
{
    const FString Models=FPaths::ProjectContentDir()/TEXT("FlightModels");
    FGliderDynamics Fox;
    if (!TestTrue(TEXT("Load Fox departure model"),Fox.LoadModel(Models/TEXT("Fox7m.json")))) return false;
    Fox.SetWind(FVector::ZeroVector);

    auto Acceleration=[&](double Degrees,double Elevator=0.,double Rudder=0.)
    {
        FVector Linear,Angular;
        Fox.Accelerations(FQuat(FVector(0,1,0),FMath::DegreesToRadians(Degrees)),
            FVector(20,0,0),FVector::ZeroVector,1000.,Elevator,Rudder,0.,0.,Linear,Angular);
        return TPair<FVector,FVector>(Linear,Angular);
    };
    // This sweep is independent of whether a sustained spin equilibrium exists.
    // Lift and drag must remain finite and neutral surfaces must not generate
    // translational energy, including reverse flow beyond the stored polar.
    for (int32 Degrees=-180;Degrees<=180;Degrees+=3)
    {
        const auto Forces=Acceleration(Degrees);
        if (Forces.Key.ContainsNaN() || Forces.Value.ContainsNaN())
        {
            AddError(FString::Printf(TEXT("Fox departure force is not finite at %d degrees"),Degrees));
            return false;
        }
        const double TranslationalPower=FVector::DotProduct(Forces.Key-FVector(0,0,9.80665),FVector(20,0,0));
        TestTrue(TEXT("Neutral aerodynamics do not propel straight translating flight"),TranslationalPower<=1.e-6);
        TestTrue(TEXT("Symmetric stalled flight has no invented lateral force or spin"),
            FMath::Abs(Forces.Key.Y)<1.e-8 && FMath::Abs(Forces.Value.X)<1.e-8 && FMath::Abs(Forces.Value.Z)<1.e-8);
    }
    const auto BeforeWrap=Acceleration(180.-1.e-5),AfterWrap=Acceleration(-180.+1.e-5);
    TestTrue(TEXT("Reverse-flow force is continuous across the incidence wrap"),BeforeWrap.Key.Equals(AfterWrap.Key,.01));
    TestTrue(TEXT("Reverse-flow pitching moment is continuous across the incidence wrap"),BeforeWrap.Value.Equals(AfterWrap.Value,.01));

    // With no translational motion, a passive airframe may dissipate rotational
    // energy, not create it. The gyroscopic term has zero scalar product with
    // angular velocity, so I*angular_acceleration is sufficient for this test.
    for (const FVector Rates : {FVector(.5,0,0),FVector(0,.5,0),FVector(0,0,.5),FVector(.2,.4,.6),FVector(0,-1.,.5)})
    {
        FVector Linear,Angular;
        Fox.Accelerations(FQuat::Identity,FVector::ZeroVector,Rates,1000.,0.,0.,0.,0.,Linear,Angular);
        TestFalse(TEXT("Rotating at zero forward speed stays finite"),Linear.ContainsNaN() || Angular.ContainsNaN());
        TestTrue(TEXT("Neutral rotation in still air cannot create rotational energy"),
            FVector::DotProduct(Fox.Inertia*Angular,Rates)<=1.e-6);
        if (Rates.Equals(FVector(0,.5,0)) || Rates.Equals(FVector(0,0,.5)))
            TestTrue(TEXT("Moving tail and fin dissipate rotation even without forward airspeed"),
                FVector::DotProduct(Fox.Inertia*Angular,Rates)<-1.e-6);
    }
    const double AttachedElevator=FMath::Abs(Acceleration(5.,.5).Value.Y-Acceleration(5.,-.5).Value.Y);
    const double StalledElevator=FMath::Abs(Acceleration(75.,.5).Value.Y-Acceleration(75.,-.5).Value.Y);
    const double AttachedRudder=FMath::Abs(Acceleration(5.,0.,.5).Value.Z-Acceleration(5.,0.,-.5).Value.Z);
    const double StalledRudder=FMath::Abs(Acceleration(75.,0.,.5).Value.Z-Acceleration(75.,0.,-.5).Value.Z);
    TestTrue(TEXT("Attached elevator and rudder retain authority"),AttachedElevator>.01 && AttachedRudder>.01);
    TestTrue(TEXT("Deep-stall elevator authority differs from attached flow"),StalledElevator<.5*AttachedElevator);
    TestTrue(TEXT("Fin authority follows its reduced forward flow at high incidence"),StalledRudder<.5*AttachedRudder);
    UE_LOG(LogTemp,Display,TEXT("FOX_DEPARTURE_AUTHORITY elevator_attached=%.4f elevator_stalled=%.4f rudder_attached=%.4f rudder_stalled=%.4f"),
        AttachedElevator,StalledElevator,AttachedRudder,StalledRudder);

    // Enter from ordinary forward flight using only the pilot's controls.
    // No preloaded spin rate, stalled initial attitude or imposed spin torque.
    // Assess ten seconds rather than instantaneous roll/yaw-rate peaks; a
    // steep spiral with low incidence cannot satisfy these conditions.
    for (const int32 Cg : {160,170})
    for (const int32 Direction : {-1,1})
    {
        FGliderDynamics Spin=Fox;
        Spin.SetLoading({24000,Cg});
        Spin.LaunchFrom(FTransform(FRotator(3.5,0,0),FVector(0,0,100000)),FVector(14,0,0));
        double MeanAlpha=0.,MeanPitch=0.,MeanAbsPitch=0.,MeanRotation=0.,MeanSink=0.,MeanSpeed=0.;
        int32 Samples=0,HighIncidenceSamples=0,RotatingSamples=0;
        for (int32 I=0;I<120*30;++I)
        {
            Spin.Step(-1.,Direction,0.);
            if (Spin.WorldPose().ContainsNaN() || !FMath::IsFinite(Spin.Airspeed()))
            {
                AddError(TEXT("Fox control-only spin entry diverged"));
                return false;
            }
            if (I<120*20) continue;
            const FVector Air=Spin.Attitude.UnrotateVector(Spin.VelocityNED()-Spin.LocalWind());
            const double Alpha=FMath::RadiansToDegrees(FMath::Atan2(Air.Z,Air.X));
            const double Pitch=Spin.WorldPose().Rotator().Pitch,Rotation=Spin.AngularVelocityWorld().Z;
            MeanAlpha+=Alpha; MeanPitch+=Pitch; MeanAbsPitch+=FMath::Abs(Pitch);
            MeanRotation+=Rotation; MeanSink+=Spin.SinkRate(); MeanSpeed+=Spin.Airspeed(); ++Samples;
            if (Alpha>40.) ++HighIncidenceSamples;
            if (Rotation*Direction<-.5) ++RotatingSamples;
        }
        MeanAlpha/=Samples; MeanPitch/=Samples; MeanAbsPitch/=Samples;
        MeanRotation/=Samples; MeanSink/=Samples; MeanSpeed/=Samples;
        const double HighFraction=double(HighIncidenceSamples)/Samples,RotationFraction=double(RotatingSamples)/Samples;
        UE_LOG(LogTemp,Display,TEXT("FOX_CONTROL_ENTRY cg=%d direction=%d mean_alpha=%.3f mean_pitch=%.3f mean_abs_pitch=%.3f world_rotation=%.3f mean_sink=%.3f mean_speed=%.3f high_alpha_fraction=%.3f rotation_fraction=%.3f"),
            Cg,Direction,MeanAlpha,MeanPitch,MeanAbsPitch,MeanRotation,MeanSink,MeanSpeed,HighFraction,RotationFraction);
        TestTrue(TEXT("Controls alone develop a sustained high-incidence spin rather than a spiral"),MeanAlpha>40. && HighFraction>.6);
        TestTrue(TEXT("The sustained spin remains flattened on average"),MeanAbsPitch<35. && MeanSink>1.);
        TestTrue(TEXT("Spin rotation retains its commanded direction over the observation window"),
            MeanRotation*Direction<-.5 && RotationFraction>.8);

        // Compare the sustained state above with a corrective-control exit.
        // This is an automated simulation scenario, not a real-aircraft
        // instruction or a claim that every CG/entry is recoverable.
        for (int32 I=0;I<1200;++I) Spin.Step(I<600?.2:0.,I<600?-Direction:0.,0.);
        const FVector RecoveredAir=Spin.Attitude.UnrotateVector(Spin.VelocityNED()-Spin.LocalWind());
        const double RecoveredAlpha=FMath::RadiansToDegrees(FMath::Atan2(RecoveredAir.Z,RecoveredAir.X));
        // Arrest rotation about the spin axis; a legitimate dive pull-out can
        // retain a pitch rate after the stalled rotation has already stopped.
        const double RecoveredRotation=FMath::Abs(Spin.AngularVelocityWorld().Z);
        UE_LOG(LogTemp,Display,TEXT("FOX_CONTROL_RECOVERY cg=%d direction=%d alpha=%.3f vertical_rotation=%.3f speed=%.3f height_loss=%.3f"),
            Cg,Direction,RecoveredAlpha,RecoveredRotation,Spin.Airspeed(),1000.-Spin.Altitude());
        TestFalse(TEXT("Recovery from the control-entered spin stays finite"),Spin.WorldPose().ContainsNaN());
        TestTrue(TEXT("Corrective controls recover attached flight and arrest spin rotation"),
            FMath::Abs(RecoveredAlpha)<15. && RecoveredRotation<.3 && Spin.Airspeed()>10.);
    }

    // Seed genuinely stalled motion about world vertical. Merely starting at
    // 19 degrees and observing a peak body roll rate does not test recovery
    // from this state. The forward CG avoids asserting that all flat states,
    // including aft-CG states, must be recoverable.
    for (const int32 Direction : {-1,1})
    {
        FGliderDynamics Recovery=Fox;
        Recovery.SetLoading({24000,150});
        Recovery.LaunchFrom(FTransform(FRotator(-20,0,0),FVector(0,0,100000)),FVector(0,0,-12));
        Recovery.Omega=Recovery.Attitude.UnrotateVector(FVector(0,0,-Direction*.8));
        FGliderDynamics Held=Recovery;
        double CleanRun=0.,RecoveryTime=-1.;
        double HeldRotation=0.,ReleasedRotation=0.,HeldAlpha=0.,HeldSink=0.;
        int32 LateSamples=0;
        for (int32 I=0;I<120*30;++I)
        {
            const bool Correcting=I<480;
            Recovery.Step(Correcting?.5:0.,Correcting?-Direction*.8:0.,0.);
            Held.Step(-1.,Direction*.8,0.);
            if (Recovery.WorldPose().ContainsNaN() || !FMath::IsFinite(Recovery.Airspeed())
                || Held.WorldPose().ContainsNaN() || !FMath::IsFinite(Held.Airspeed()))
            {
                AddError(TEXT("Fox counter-rudder recovery diverged"));
                return false;
            }
            const FVector LocalAir=Recovery.Attitude.UnrotateVector(Recovery.VelocityNED()-Recovery.LocalWind());
            const double Alpha=FMath::RadiansToDegrees(FMath::Atan2(LocalAir.Z,LocalAir.X));
            const bool Clean=FMath::Abs(Alpha)<15. && Recovery.AngularVelocityWorld().Size()<.4 && Recovery.Airspeed()>10.;
            CleanRun=Clean ? CleanRun+Recovery.TimeStep : 0.;
            if (RecoveryTime<0. && CleanRun>=2.) RecoveryTime=(I+1)*Recovery.TimeStep;
            if (I>=120*25)
            {
                const FVector HeldAir=Held.Attitude.UnrotateVector(Held.VelocityNED()-Held.LocalWind());
                HeldAlpha+=FMath::RadiansToDegrees(FMath::Atan2(HeldAir.Z,HeldAir.X));
                HeldRotation+=FMath::Abs(Held.AngularVelocityWorld().Z);
                ReleasedRotation+=FMath::Abs(Recovery.AngularVelocityWorld().Z);
                HeldSink+=Held.SinkRate(); ++LateSamples;
            }
        }
        UE_LOG(LogTemp,Display,TEXT("FOX_SEEDED_RECOVERY direction=%d recovered_after_s=%.3f final_speed=%.3f height_loss=%.3f held_alpha=%.3f held_sink=%.3f held_world_rotation=%.3f recovered_world_rotation=%.3f"),
            Direction,RecoveryTime,Recovery.Airspeed(),1000.-Recovery.Altitude(),HeldAlpha/LateSamples,HeldSink/LateSamples,
            HeldRotation/LateSamples,ReleasedRotation/LateSamples);
        TestTrue(TEXT("Forward-CG Fox recovers from seeded stalled rotation with corrective controls"),RecoveryTime>0.);
        TestTrue(TEXT("Corrective controls leave less rotation than holding the stalled inputs"),ReleasedRotation<.5*HeldRotation);
    }

    // Reloading or relaunching after a departure must not retain hidden wake,
    // angular-rate or servo history. Preserve model/loading settings explicitly.
    for (const TCHAR* Name : {TEXT("Topy4522"),TEXT("K3600"),TEXT("VJX2022"),TEXT("Fox7m"),TEXT("JS3_9m"),
        TEXT("Snipe2"),TEXT("LongBowMk2"),TEXT("Apus4"),TEXT("Nubis")})
    {
        FGliderDynamics Reference;
        if (!TestTrue(TEXT("Load departure reset model"),Reference.LoadModel(Models/(FString(Name)+TEXT(".json"))))) return false;
        Reference.SetWind(FVector::ZeroVector);
        auto Disturb=[](FGliderDynamics& Model)
        {
            Model.LaunchFrom(FTransform(FRotator(45,0,20),FVector(0,0,100000)),FVector(14,0,0));
            for (int32 I=0;I<480;++I) Model.Step(-1.,.8,.3);
        };
        auto SameMotion=[](const FGliderDynamics& A,const FGliderDynamics& B)
        {
            return A.WorldPose().Equals(B.WorldPose(),1.e-7) && A.VelocityNED().Equals(B.VelocityNED(),1.e-8)
                && A.AngularVelocityWorld().Equals(B.AngularVelocityWorld(),1.e-8);
        };
        FGliderDynamics Disturbed=Reference,Fresh=Reference;
        Disturb(Disturbed);
        Disturbed.Reset(FVector::ZeroVector);
        Fresh.Reset(FVector::ZeroVector);
        for (int32 I=0;I<240;++I) { Disturbed.Step(0.,0.,0.); Fresh.Step(0.,0.,0.); }
        TestTrue(FString::Printf(TEXT("%s reset discards departure history"),Name),SameMotion(Disturbed,Fresh));
        Disturb(Disturbed);
        Fresh=Reference;
        const FTransform Launch(FRotator(2,0,0),FVector(0,0,100000));
        Disturbed.LaunchFrom(Launch,FVector(15,0,0));
        Fresh.LaunchFrom(Launch,FVector(15,0,0));
        for (int32 I=0;I<240;++I) { Disturbed.Step(0.,0.,0.); Fresh.Step(0.,0.,0.); }
        TestTrue(FString::Printf(TEXT("%s relaunch discards departure history"),Name),SameMotion(Disturbed,Fresh));
    }
    return !HasAnyErrors();
}
#endif
