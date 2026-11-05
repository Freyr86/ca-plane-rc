#include "GliderDynamics.h"
#include "Algo/BinarySearch.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
    using FJson= TSharedPtr<FJsonObject>;
    bool Number(const FJson& Object,const TCHAR* Name,double& Out)
    {
        return Object->TryGetNumberField(Name,Out) && FMath::IsFinite(Out);
    }
    bool Numbers(const TArray<TSharedPtr<FJsonValue>>& Values,TArray<double>& Out)
    {
        Out.Reset(Values.Num());
        for (const auto& V:Values)
        {
            double N;
            if (!V->TryGetNumber(N) || !FMath::IsFinite(N)) return false;
            Out.Add(N);
        }
        return true;
    }
    bool Increasing(const TArray<double>& X)
    {
        if (X.Num()<2) return false;
        for (int32 I=1;I<X.Num();++I) if (X[I]<=X[I-1]) return false;
        return true;
    }
    int32 Interval(const TArray<double>& X,double Value,double& Fraction)
    {
        const int32 I=FMath::Clamp(Algo::UpperBound(X,Value)-1,0,X.Num()-2);
        Fraction=FMath::Clamp((Value-X[I])/(X[I+1]-X[I]),0.,1.);
        return I;
    }
    FQuat Turn(FQuat Start,FVector Rates,double Time)
    {
        const double Speed=Rates.Size();
        return Speed<1.e-12 ? Start : (Start*FQuat(Rates/Speed,Speed*Time)).GetNormalized();
    }
    // Mirror polar vectors across Z to go from NED to Unreal. The vector
    // part of a quaternion is axial, so its X and Y change sign instead.
    FQuat MirrorAttitude(FQuat Q) {return FQuat(-Q.X,-Q.Y,Q.Z,Q.W);}
}
FString FGliderDynamics::TopyModelPath()
{
    return FPaths::Combine(FPaths::ProjectContentDir(),TEXT("FlightModels/Topy4522.json"));
}
bool FGliderDynamics::LoadModel(const FString& Filename)
{
    FString Data;
    FJson Json;
    if (!FFileHelper::LoadFileToString(Data,*Filename) ||
        !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Data),Json) || !Json) return false;
    FGliderDynamics M;
    if(Json->HasField(TEXT("impact_resistance")) && (!Number(Json,TEXT("impact_resistance"),M.ImpactResistance) || M.ImpactResistance<=0.)) return false;
    double Schema;
    if (!Number(Json,TEXT("schema"),Schema) || Schema!=1. ||
        !Number(Json,TEXT("mass_kg"),M.Mass) || !Number(Json,TEXT("wing_area_m2"),M.Area) ||
        !Number(Json,TEXT("span_m"),M.Span) || !Number(Json,TEXT("chord_m"),M.Chord) ||
        !Number(Json,TEXT("servo_travel_per_s"),M.ServoRate) ||
        FMath::Min3(M.Mass,M.Area,M.Span)<=0. || M.Chord<=0. || M.ServoRate<=0.) return false;
    if (!Number(Json,TEXT("side_beta"),M.SideBeta) || !Number(Json,TEXT("side_rudder"),M.SideRudder) ||
        !Number(Json,TEXT("drag_elevator_squared"),M.DragElevator) || !Number(Json,TEXT("drag_rudder_squared"),M.DragRudder)) return false;
    const auto ReadMoment=[&](const TCHAR* Name,FMoment& C)
    {
        const FJson* Obj;
        return Json->TryGetObjectField(Name,Obj) && Number(*Obj,TEXT("bias"),C.Bias) &&
            Number(*Obj,TEXT("angle"),C.Angle) && Number(*Obj,TEXT("rate"),C.Rate) && Number(*Obj,TEXT("control"),C.Control);
    };
    if (!ReadMoment(TEXT("roll"),M.Roll) || !ReadMoment(TEXT("pitch"),M.Pitch) || !ReadMoment(TEXT("yaw"),M.Yaw)) return false;
    if (Json->HasField(TEXT("extra_down_elevator_moment")) &&
        !Number(Json,TEXT("extra_down_elevator_moment"),M.ExtraDownElevatorMoment)) return false;
    const TArray<TSharedPtr<FJsonValue>>* Items;
    TArray<double> Row;
    if (!Json->TryGetArrayField(TEXT("inertia_diagonal_kg_m2"),Items) || !Numbers(*Items,Row) || Row.Num()!=3 ||
        FMath::Min3(Row[0],Row[1],Row[2])<=0.) return false;
    M.Inertia=FVector(Row[0],Row[1],Row[2]);
    if (!Json->TryGetArrayField(TEXT("drag_speeds_mps"),Items) || !Numbers(*Items,M.Speeds) ||
        !Increasing(M.Speeds) || M.Speeds[0]<=0.) return false;
    if (!Json->TryGetArrayField(TEXT("polar_alpha_rad_cl_cd"),Items)) return false;
    for (const auto& Entry:*Items)
    {
        const TArray<TSharedPtr<FJsonValue>>* Columns;
        if (!Entry->TryGetArray(Columns) || !Numbers(*Columns,Row) || Row.Num()!=M.Speeds.Num()+2) return false;
        M.Angles.Add(Row[0]); M.Lift.Add(Row[1]);
        for (int32 I=2;I<Row.Num();++I)
        {
            if (Row[I]<0.) return false;
            M.Drag.Add(Row[I]);
        }
    }
    if (!Increasing(M.Angles)) return false;
    Json->TryGetNumberField(TEXT("discus_launch_pitch_trim"),M.LaunchPitchTrim);
    Json->TryGetNumberField(TEXT("spoiler_drag_coefficient"),M.SpoilerDrag);
    Json->TryGetBoolField(TEXT("conventional_tail"), M.ConventionalTail);
    Json->TryGetBoolField(TEXT("fixed_fin"), M.FixedFin);
    Json->TryGetNumberField(TEXT("aileron_roll_coefficient"), M.AileronEffect);
    if (Json->HasField(TEXT("flaps")))
    {
        const FJson* Flaps;
        double Down=0., Up=0.;
        if (!Json->TryGetObjectField(TEXT("flaps"),Flaps) ||
            !Number(*Flaps,TEXT("max_down_deg"),Down) || Down<=0. || Down>90. ||
            !Number(*Flaps,TEXT("affected_wing_fraction"),M.FlapAreaFraction) ||
            M.FlapAreaFraction<=0. || M.FlapAreaFraction>1.) return false;
        if ((*Flaps)->HasField(TEXT("max_up_deg")) &&
            (!Number(*Flaps,TEXT("max_up_deg"),Up) || Up<0. || Up>90.)) return false;
        M.StandardTravel = {FMath::RoundToInt32(Up),FMath::RoundToInt32(Down)};
        M.Travel = M.StandardTravel;
    }
    Json->TryGetBoolField(TEXT("elevons"),M.Elevons);
    Json->TryGetNumberField(TEXT("aileron_yaw_coefficient"),M.YawFromAilerons);
    M.StandardMass = M.Mass;
    int32 MotorWatts=FMath::RoundToInt32(150.*M.StandardMass);
    const bool Powered=Json->TryGetNumberField(TEXT("motor_watts"),MotorWatts) && MotorWatts>0;
    M.SetMotor({Powered,MotorWatts});
    M.StandardInertia = M.Inertia;
    if (Json->HasField(TEXT("reference_cg_from_root_le_mm")) &&
        !Json->TryGetNumberField(TEXT("reference_cg_from_root_le_mm"), M.ReferenceCgMm)) return false;
    *this=MoveTemp(M);
    Reset();
    return true;
}
FGliderLoading FGliderDynamics::LimitLoading(FGliderLoading Setting) const
{
    const auto Standard = StandardLoading();
    Setting.MassGrams = FMath::Clamp(Setting.MassGrams, FMath::RoundToInt32(Standard.MassGrams * .75),
                                     FMath::RoundToInt32(Standard.MassGrams * 1.375));
    Setting.CgMm = FMath::Clamp(Setting.CgMm, ReferenceCgMm - 10, ReferenceCgMm + 10);
    return Setting;
}

void FGliderDynamics::SetLoading(FGliderLoading Setting)
{
    if (StandardMass <= 0.) return;
    Setting = LimitLoading(Setting);
    Mass = Setting.MassGrams * .001;
    // Fixed radii of gyration: distribute mass proportionally around the new CG.
    Inertia = StandardInertia * (Mass / StandardMass);
    CgShift = (Setting.CgMm - ReferenceCgMm) * .001;
}

void FGliderDynamics::SetControlGains(FGliderControlGains Setting)
{
    Gains = {FMath::Clamp(Setting.Roll,-500,500),FMath::Clamp(Setting.Pitch,-500,500),FMath::Clamp(Setting.Yaw,-500,500)};
}

void FGliderDynamics::AdvanceFlaps(double Command,double DeltaSeconds,double Crow)
{
    if (!HasFlaps()) return;
    Crow = FMath::Clamp(Crow,0.,1.);
    const double Manual = FMath::Clamp(Command,-1.,1.)*(Command<0. ? Travel.Up : Travel.Down);
    const double Target = FMath::Lerp(Manual,double(Travel.Down),Crow);
    const double Slew = 90.*FMath::Max(DeltaSeconds,0.);
    const double OldFlap = FlapAngle, OldCrow = CrowServo;
    FlapAngle += FMath::Clamp(Target-FlapAngle,-Slew,Slew);
    CrowServo += FMath::Clamp(Crow-CrowServo,-Slew/80.,Slew/80.);
    ServoTravel += FMath::Max(FMath::Abs(FlapAngle-OldFlap)/45., FMath::Abs(CrowServo-OldCrow));
    const double Angle = FMath::DegreesToRadians(FlapAngle);
    // Partial-span plain flaps: lift grows first, then pressure drag dominates.
    FlapDeltaLift = .95*FlapAreaFraction*FMath::Sin(2.*Angle) - .20*CrowServo; // Raised outboard ailerons unload the tips.
    FlapDeltaDrag = .78*FlapAreaFraction*FMath::Square(FMath::Sin(Angle)) + .10*CrowServo*CrowServo;
    FlapDeltaPitch = -.06*FlapAreaFraction*FMath::Sin(Angle);
}
void FGliderDynamics::Reset(FVector WindNED)
{
    AppliedMotorWatts = 0.;
    ServoTravel = 0.;
    CrowServo=FlapAngle=FlapDeltaLift=FlapDeltaDrag=FlapDeltaPitch=0.;
    LaunchTrimTimeLeft=0.;
    SpoilerPosition=0.;
    SetWind(WindNED);
    Position=FVector(0,0,-80);
    const double Theta=FMath::DegreesToRadians(-1.4), Alpha=FMath::DegreesToRadians(3.5);
    Attitude=FQuat(FVector(0,1,0),Theta);
    // The original hand launch specified ground velocity before enabling the breeze.
    Velocity=Attitude.RotateVector(7.*FVector(FMath::Cos(Alpha),0,FMath::Sin(Alpha)));
    Omega=FVector::ZeroVector;
    Left=Right=AileronServo=0.;
    Stopped=false;
}
void FGliderDynamics::Accelerations(FQuat BodyToWorld,FVector WorldVelocity,FVector Rates,double Height,
    double PitchServo,double YawServo,double RollServo,double ShaftWatts,FVector& Linear,FVector& Angular) const
{
    const FVector Air=BodyToWorld.UnrotateVector(WorldVelocity-Wind);
    const double V=Air.Size();
    const double Alpha=FMath::Atan2(Air.Z,Air.X);
    const double Beta=FMath::Atan2(Air.Y,FMath::Sqrt(Air.X*Air.X+Air.Z*Air.Z));
    double AngleFraction, SpeedFraction;
    const int32 AngleRow = Interval(Angles, Alpha, AngleFraction);
    const int32 SpeedColumn = Interval(Speeds, V, SpeedFraction);
    const auto DragAtSpeed = [&](int32 Row)
    {
        const int32 Index = Row * Speeds.Num() + SpeedColumn;
        return FMath::Lerp(Drag[Index], Drag[Index + 1], SpeedFraction);
    };
    const double CleanLift = FMath::Lerp(Lift[AngleRow], Lift[AngleRow + 1], AngleFraction);
    const double CL = CleanLift*(1.-.23*SpoilerPosition) + FlapDeltaLift*FMath::Clamp(1.-FMath::Abs(Alpha)/.52,0.,1.);
    const double ExtraInducedDrag = (CL*CL-CleanLift*CleanLift)*Area/(PI*.85*Span*Span);
    // Scale control effectiveness after the servo/mixer limits so values above
    // 100% remain effective at full stick. Neutral stability derivatives stay physical.
    const double PitchControl = PitchServo * (Gains.Pitch * .01);
    const double YawControl = YawServo * (Gains.Yaw * .01);
    const double RollControl = YawServo * (Gains.Roll * .01);
    const double CD = FMath::Lerp(DragAtSpeed(AngleRow), DragAtSpeed(AngleRow + 1), AngleFraction)
                    + SpoilerDrag*SpoilerPosition + FlapDeltaDrag + ExtraInducedDrag + DragElevator * PitchControl * PitchControl + DragRudder * YawControl * YawControl;
    const double CY=SideBeta*Beta+SideRudder*YawControl;
    double Sa,Ca,Sb,Cb;
    FMath::SinCos(&Sa,&Ca,Alpha); FMath::SinCos(&Sb,&Cb,Beta);
    const FVector Flow(Ca*Cb,Sb,Sa*Cb), Side(-Ca*Sb,Cb,-Sa*Sb), Down(-Sa,0,Ca);
    const double Rho=1.225*FMath::Pow(1.-2.25577e-5*FMath::Clamp(Height,-1000.,11000.),4.25588);
    const double QS=.5*Rho*V*V*Area;
    FVector Force=QS*(-CD*Flow+CY*Side-CL*Down);
    const double Inv2V=V>1.e-6 ? .5/V : 0.;
    // An updraft reaching one wing first acts through the same roll damping
    // derivative as a spanwise velocity difference caused by body rotation.
    const double AirRollRate = BodyToWorld.UnrotateVector(WingWindDifference).Z / Span;
    const FVector Torque=FVector::CrossProduct(FVector(CgShift, 0., 0.), Force) + QS*FVector(
        Span*(Roll.Evaluate(Beta,Span*Inv2V*(Rates.X-AirRollRate),RollControl) + AileronEffect*RollServo*(Gains.Roll*.01)),
        Chord*(Pitch.Evaluate(Alpha,Chord*Inv2V*Rates.Y,PitchControl)+FlapDeltaPitch
            +ExtraDownElevatorMoment*FMath::Clamp(PitchControl,0.,1.)*FMath::Max(PitchControl,0.)),
        Span*(Yaw.Evaluate(Beta,Span*Inv2V*Rates.Z,YawControl)+YawFromAilerons*RollServo*Gains.Roll*.01));
    // Generic folding propeller on the centreline. An effective slipstream
    // speed of 12 m/s limits static thrust; at speed thrust falls with airspeed.
    if (ShaftWatts>0.)
        Force.X += .75*ShaftWatts/FMath::Sqrt(144.+FMath::Square(FMath::Max(0.,Air.X)));
    Linear=BodyToWorld.RotateVector(Force/Mass)+FVector(0,0,9.80665);
    Angular=(Torque-FVector::CrossProduct(Rates,Inertia*Rates))/Inertia;
}
void FGliderDynamics::Step(double Elevator,double Rudder)
{
    Step(Elevator,Rudder,0.);
}
void FGliderDynamics::Step(double Elevator,double Rudder,double Aileron,double Throttle,const FVector& TowForceNED)
{
    if (Stopped) return;
    if (LaunchTrimTimeLeft > 0.)
    {
        // Fade the DLG speed preset out on the first climb. Full pilot input
        // always overrides it, and a later dive does not re-arm it.
        const double Blend = FMath::Clamp((Airspeed()-6.5)/3.5,0.,1.) * FMath::Min(1.,LaunchTrimTimeLeft/2.);
        Elevator += LaunchPitchTrim * Blend * (1.-FMath::Abs(Elevator));
        LaunchTrimTimeLeft = Blend > 0. ? LaunchTrimTimeLeft - TimeStep : 0.;
    }
    const double OldLeft=Left, OldRight=Right, OldAileron=AileronServo;
    AdvanceServos(Elevator,Rudder,Aileron,TimeStep);
    const double Half = .5 * TimeStep;
    FVector Accel,Angular;
    AppliedMotorWatts = Motor.Enabled ? Motor.Watts*FMath::Clamp(Throttle,0.,1.) : 0.;
    Accelerations(Attitude,Velocity,Omega,-Position.Z,.5*(OldLeft+OldRight),.5*(OldLeft-OldRight),OldAileron,AppliedMotorWatts,Accel,Angular);
    Accel += TowForceNED / Mass;
    const FVector MidVelocity=Velocity+Half*Accel, MidRates=Omega+Half*Angular;
    const FQuat MidAttitude=Turn(Attitude,Omega,Half);
    Accelerations(MidAttitude,MidVelocity,MidRates,-Position.Z-Half*Velocity.Z,
        .25*(OldLeft+OldRight+Left+Right),.25*(OldLeft-OldRight+Left-Right),.5*(OldAileron+AileronServo),AppliedMotorWatts,Accel,Angular);
    Accel += TowForceNED / Mass;
    Position+=TimeStep*MidVelocity;
    Velocity+=TimeStep*Accel;
    Attitude=Turn(Attitude,MidRates,TimeStep);
    Omega+=TimeStep*Angular;
}
void FGliderDynamics::AdvanceServos(double Elevator,double Rudder,double Aileron,double DeltaSeconds)
{
    // Aircraft command convention: positive elevator deflects down; positive
    // rudder input here is the same tail command used in the reference polar model.
    if (!HasRudder()) Rudder = 0.;
    const double MaxTravel = ServoRate * DeltaSeconds;
    const auto MoveServo = [MaxTravel](double Current, double Requested)
    {
        const double Target = FMath::Clamp(Requested, -1., 1.);
        return Current + FMath::Clamp(Target - Current, -MaxTravel, MaxTravel);
    };
    double NextLeft, NextRight;
    if (Elevons)
    {
        NextLeft = MoveServo(Left, Elevator + Aileron);
        NextRight = MoveServo(Right, Elevator - Aileron);
    }
    else if (ConventionalTail)
    {
        const double PitchServo = MoveServo(.5*(Left+Right), Elevator);
        const double YawServo = MoveServo(.5*(Left-Right), Rudder);
        NextLeft = PitchServo + YawServo;
        NextRight = PitchServo - YawServo;
    }
    else
    {
        NextLeft = MoveServo(Left, Elevator + Rudder);
        NextRight = MoveServo(Right, Elevator - Rudder);
    }
    const double NextAileron = Elevons ? .5*(NextLeft-NextRight) : MoveServo(AileronServo,Aileron);
    ServoTravel += FMath::Max3(FMath::Abs(NextLeft-Left),FMath::Abs(NextRight-Right),FMath::Abs(NextAileron-AileronServo));
    AileronServo = NextAileron;
    Left = NextLeft;
    Right = NextRight;
}
FTransform FGliderDynamics::WorldPose() const
{
    const FVector Datum = Position + Attitude.GetForwardVector() * CgShift;
    return FTransform(MirrorAttitude(Attitude),100.*FVector(Datum.X,Datum.Y,-Datum.Z));
}
void FGliderDynamics::StopAt(const FTransform& Contact)
{
    AppliedMotorWatts = 0.;
    const FVector P=Contact.GetLocation()*.01;
    Attitude=MirrorAttitude(Contact.GetRotation());
    Position=FVector(P.X,P.Y,-P.Z) - Attitude.GetForwardVector() * CgShift;
    Velocity=Omega=FVector::ZeroVector;
    Stopped=true;
}

void FGliderDynamics::LaunchFrom(const FTransform& Pose, const FVector& WorldVelocityMps)
{
    StopAt(Pose);
    Velocity=FVector(WorldVelocityMps.X,WorldVelocityMps.Y,-WorldVelocityMps.Z);
    LaunchTrimTimeLeft = LaunchPitchTrim != 0. && WorldVelocityMps.Size() > 20. ? 6. : 0.;
    Left=Right=AileronServo=0.;
    Stopped=false;
}
