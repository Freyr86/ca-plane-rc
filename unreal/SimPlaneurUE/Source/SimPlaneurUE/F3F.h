#pragma once
#include "CoreMinimal.h"

// World centimetres; timing uses simulated seconds, never rendering time.
struct FF3FCourse
{
    FVector A=FVector::ZeroVector, B=FVector::ZeroVector;
    FVector FlyingSide=FVector::ZeroVector;
    bool Installed=false, Gates=true;
    FVector Axis() const { return (B-A).GetSafeNormal2D(); }
    bool Valid() const;
    void Save(const FString& Ini) const;
    void Load(const FString& Ini);
};

struct FF3FRun
{
    enum class EState { Idle, Ready, Entry, Racing, Clearance, Finished, Invalid };
    EState State=EState::Idle;
    int32 Legs=0, Signals=0, SafetyCrossings=0;
    double Now=0., LaunchTime=0., StartTime=0., FinishTime=0., LaunchZ=0.;
    TArray<double> Splits;
    FString Reason;
    bool Late=false, WeatherRetry=false, PendingTurn=false;
    double BadWindSeconds=0.;
    void Arm() { *this=FF3FRun(); State=EState::Ready; }
    void Launch(double Height);
    void Invalidate(const FString& Why);
    bool Active() const { return State==EState::Entry || State==EState::Racing || State==EState::Clearance; }
    double Time() const;
    // Support points include wings, nose and tail; any part crossing counts.
    void Step(const FF3FCourse& Course,const TArray<FVector>& Before,const TArray<FVector>& After,double Dt,bool JudgeVisible=true);
    void Weather(bool WindOK,bool RainOrNight,double Dt);
};
