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
    Json->TryGetBoolField(TEXT("conventional_tail"), M.ConventionalTail);
    M.StandardMass = M.Mass;
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

void FGliderDynamics::Reset(FVector WindNED)
{
    SetWind(WindNED);
    Position=FVector(0,0,-80);
    const double Theta=FMath::DegreesToRadians(-1.4), Alpha=FMath::DegreesToRadians(3.5);
    Attitude=FQuat(FVector(0,1,0),Theta);
    // The original hand launch specified ground velocity before enabling the breeze.
    Velocity=Attitude.RotateVector(7.*FVector(FMath::Cos(Alpha),0,FMath::Sin(Alpha)));
    Omega=FVector::ZeroVector;
    Left=Right=0.;
    Stopped=false;
}
void FGliderDynamics::Accelerations(FQuat BodyToWorld,FVector WorldVelocity,FVector Rates,double Height,
    double PitchServo,double YawServo,FVector& Linear,FVector& Angular) const
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
    const double CL = FMath::Lerp(Lift[AngleRow], Lift[AngleRow + 1], AngleFraction);
    // Scale control effectiveness after the servo/mixer limits so values above
    // 100% remain effective at full stick. Neutral stability derivatives stay physical.
    const double PitchControl = PitchServo * (Gains.Pitch * .01);
    const double YawControl = YawServo * (Gains.Yaw * .01);
    const double RollControl = YawServo * (Gains.Roll * .01);
    const double CD = FMath::Lerp(DragAtSpeed(AngleRow), DragAtSpeed(AngleRow + 1), AngleFraction)
                    + DragElevator * PitchControl * PitchControl + DragRudder * YawControl * YawControl;
    const double CY=SideBeta*Beta+SideRudder*YawControl;
    double Sa,Ca,Sb,Cb;
    FMath::SinCos(&Sa,&Ca,Alpha); FMath::SinCos(&Sb,&Cb,Beta);
    const FVector Flow(Ca*Cb,Sb,Sa*Cb), Side(-Ca*Sb,Cb,-Sa*Sb), Down(-Sa,0,Ca);
    const double Rho=1.225*FMath::Pow(1.-2.25577e-5*FMath::Clamp(Height,-1000.,11000.),4.25588);
    const double QS=.5*Rho*V*V*Area;
    const FVector Force=QS*(-CD*Flow+CY*Side-CL*Down);
    const double Inv2V=V>1.e-6 ? .5/V : 0.;
    // An updraft reaching one wing first acts through the same roll damping
    // derivative as a spanwise velocity difference caused by body rotation.
    const double AirRollRate = BodyToWorld.UnrotateVector(WingWindDifference).Z / Span;
    const FVector Torque=FVector::CrossProduct(FVector(CgShift, 0., 0.), Force) + QS*FVector(
        Span*Roll.Evaluate(Beta,Span*Inv2V*(Rates.X-AirRollRate),RollControl),
        Chord*Pitch.Evaluate(Alpha,Chord*Inv2V*Rates.Y,PitchControl),
        Span*Yaw.Evaluate(Beta,Span*Inv2V*Rates.Z,YawControl));
    Linear=BodyToWorld.RotateVector(Force/Mass)+FVector(0,0,9.80665);
    Angular=(Torque-FVector::CrossProduct(Rates,Inertia*Rates))/Inertia;
}
void FGliderDynamics::Step(double Elevator,double Rudder)
{
    if (Stopped) return;
    // Aircraft command convention: positive elevator deflects down; positive
    // rudder input here is the same tail command used in the reference polar model.
    const double MaxTravel = ServoRate * TimeStep;
    const auto MoveServo = [MaxTravel](double Current, double Requested)
    {
        const double Target = FMath::Clamp(Requested, -1., 1.);
        return Current + FMath::Clamp(Target - Current, -MaxTravel, MaxTravel);
    };
    double NextLeft, NextRight;
    if (ConventionalTail)
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
    const double Half = .5 * TimeStep;
    FVector Accel,Angular;
    Accelerations(Attitude,Velocity,Omega,-Position.Z,.5*(Left+Right),.5*(Left-Right),Accel,Angular);
    const FVector MidVelocity=Velocity+Half*Accel, MidRates=Omega+Half*Angular;
    const FQuat MidAttitude=Turn(Attitude,Omega,Half);
    Accelerations(MidAttitude,MidVelocity,MidRates,-Position.Z-Half*Velocity.Z,
        .25*(Left+Right+NextLeft+NextRight),.25*(Left-Right+NextLeft-NextRight),Accel,Angular);
    Position+=TimeStep*MidVelocity;
    Velocity+=TimeStep*Accel;
    Attitude=Turn(Attitude,MidRates,TimeStep);
    Omega+=TimeStep*Angular;
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
    Left=Right=0.;
    Stopped=false;
}
