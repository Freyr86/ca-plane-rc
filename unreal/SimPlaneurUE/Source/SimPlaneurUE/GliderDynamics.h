#pragma once
#include "CoreMinimal.h"

struct FGliderLoading
{
    int32 MassGrams = 0;
    int32 CgMm = 0; // aft from the root leading edge
    bool operator==(const FGliderLoading&) const = default;
};

struct FGliderControlGains
{
    int32 Roll = 100, Pitch = 100, Yaw = 100;
    bool operator==(const FGliderControlGains&) const = default;
};

// SI units. World coordinates are north/east/down; body coordinates are
// forward/right/down. Only WorldPose and StopAt cross the Unreal boundary.
struct FGliderDynamics
{
    static constexpr double TimeStep=1.0/120.0;
    bool LoadModel(const FString& Filename);
    static FString TopyModelPath();
    void Reset(FVector WindNED=FVector(-2,0,0));
    void Step(double Elevator,double Rudder);
    FGliderLoading StandardLoading() const { return {FMath::RoundToInt32(StandardMass * 1000.), ReferenceCgMm}; }
    FGliderLoading Loading() const { return {FMath::RoundToInt32(Mass * 1000.), ReferenceCgMm + FMath::RoundToInt32(CgShift * 1000.)}; }
    FGliderLoading LimitLoading(FGliderLoading Setting) const;
    void SetLoading(FGliderLoading Setting);
    FGliderControlGains ControlGains() const { return Gains; }
    void SetControlGains(FGliderControlGains Setting);
    void SetWind(const FVector& WindNED, const FVector& RightMinusLeft = FVector::ZeroVector)
    {
        Wind = WindNED;
        WingWindDifference = RightMinusLeft;
    }
    FVector RightTipOffsetNED() const { return Attitude.GetRightVector() * (.5 * Span); }
    const FVector& LocalWind() const { return Wind; }
    const FVector& PositionNED() const { return Position; }
    FVector RelativeFlowNED() const { return Wind - Velocity; }
    FTransform WorldPose() const;
    void StopAt(const FTransform& Contact);
    void LaunchFrom(const FTransform& Pose, const FVector& WorldVelocityMps);
    double Airspeed() const { return Stopped ? 0. : RelativeFlowNED().Size(); }
    double SinkRate() const { return Velocity.Z; }
    double Altitude() const { return -Position.Z; }
    bool IsStopped() const {return Stopped;}

private:
    struct FMoment
    {
        double Bias=0, Angle=0, Rate=0, Control=0;
        double Evaluate(double Incidence,double ReducedRate,double Servo) const
        { return Bias+Angle*Incidence+Rate*ReducedRate+Control*Servo; }
    };
    double Mass=0, Area=0, Span=0, Chord=0, ServoRate=0;
    double SideBeta=0, SideRudder=0, DragElevator=0, DragRudder=0;
    FVector Inertia=FVector::ZeroVector;
    FVector StandardInertia = FVector::ZeroVector;
    double StandardMass = 0., CgShift = 0.;
    int32 ReferenceCgMm = 0;
    FMoment Roll,Pitch,Yaw;
    FGliderControlGains Gains;
    TArray<double> Angles, Speeds, Lift, Drag;

    FVector Position=FVector::ZeroVector, Velocity=FVector::ZeroVector, Wind=FVector::ZeroVector;
    FVector WingWindDifference = FVector::ZeroVector;
    FQuat Attitude=FQuat::Identity;
    FVector Omega=FVector::ZeroVector;
    double Left=0,Right=0;
    bool Stopped=false;
    bool ConventionalTail=false;

    void Accelerations(FQuat BodyToWorld,FVector WorldVelocity,FVector Rates,double Height,
                       double PitchServo,double YawServo,FVector& Linear,FVector& Angular) const;
};
