#include "F3F.h"
#include "Misc/ConfigCacheIni.h"

bool FF3FCourse::Valid() const
{
    return Installed && !A.ContainsNaN() && !B.ContainsNaN() && !FlyingSide.ContainsNaN()
        && FMath::Abs(FVector::Dist2D(A,B)-10000.)<1.
        && FMath::Abs(FlyingSide.Size()-1.)<.01 && FMath::Abs(FlyingSide.Z)<.001
        && FMath::Abs(FVector::DotProduct(Axis(),FlyingSide))<.001;
}
void FF3FCourse::Save(const FString& Ini) const
{
    const TCHAR* S=TEXT("SimPlaneur.F3F");
    GConfig->SetBool(S,TEXT("Installed"),Valid(),Ini);
    GConfig->SetVector(S,TEXT("A"),A,Ini); GConfig->SetVector(S,TEXT("B"),B,Ini);
    GConfig->SetVector(S,TEXT("FlyingSide"),FlyingSide,Ini);
    GConfig->SetBool(S,TEXT("Gates"),Gates,Ini); GConfig->Flush(false,Ini);
}
void FF3FCourse::Load(const FString& Ini)
{
    const TCHAR* S=TEXT("SimPlaneur.F3F");
    GConfig->GetBool(S,TEXT("Installed"),Installed,Ini);
    GConfig->GetVector(S,TEXT("A"),A,Ini); GConfig->GetVector(S,TEXT("B"),B,Ini);
    GConfig->GetVector(S,TEXT("FlyingSide"),FlyingSide,Ini);
    GConfig->GetBool(S,TEXT("Gates"),Gates,Ini);
    if(!Valid()) Installed=false;
}
void FF3FRun::Launch(double Height)
{
    if(State!=EState::Ready) return;
    LaunchZ=Height; LaunchTime=Now; State=EState::Entry;
}
void FF3FRun::Invalidate(const FString& Why)
{
    if(!Active() && State!=EState::Ready) return;
    if(State==EState::Ready || (State==EState::Entry && !Late)) StartTime=Now;
    FinishTime=Now; Reason=Why; State=EState::Invalid;
}
double FF3FRun::Time() const
{
    if(State==EState::Idle || State==EState::Ready || (State==EState::Entry && !Late)) return 0.;
    return FMath::Max(0.,(State==EState::Finished || State==EState::Clearance || State==EState::Invalid ? FinishTime : Now)-StartTime);
}
void FF3FRun::Weather(bool WindOK,bool RainOrNight,double Dt)
{
    if(!Active()) return;
    BadWindSeconds=WindOK?0.:BadWindSeconds+Dt;
    WeatherRetry |= BadWindSeconds>=20. || RainOrNight;
}
void FF3FRun::Step(const FF3FCourse& C,const TArray<FVector>& Before,const TArray<FVector>& After,double Dt,bool JudgeVisible)
{
    if(!Active() || !C.Valid() || Before.IsEmpty() || Before.Num()!=After.Num() || Dt<=0.) return;
    const double PreviousTime=Now; Now+=Dt;
    auto Extent=[&](const TArray<FVector>& Points,const FVector& Origin,const FVector& Normal)
    {
        double Value=-DBL_MAX;
        for(const FVector& P:Points) Value=FMath::Max(Value,FVector::DotProduct(P-Origin,Normal));
        return Value;
    };
    auto Crossing=[&](const FVector& Origin,const FVector& Normal,double& Fraction)
    {
        const double Old=Extent(Before,Origin,Normal), New=Extent(After,Origin,Normal);
        if(Old>=0. || New<0.) return false;
        Fraction=FMath::Clamp(-Old/(New-Old),0.,1.); return true;
    };
    double F=0.;
    // Safety side is behind the flags. Count only transitions towards spectators.
    if(State!=EState::Clearance && Crossing(C.A,-C.FlyingSide,F)) ++SafetyCrossings;
    if(State==EState::Entry)
    {
        if(Crossing(C.A,C.Axis(),F))
        {
            if(!JudgeVisible) { Invalidate(TEXT("Entrée masquée au juge A")); return; }
            const double Entry=PreviousTime+F*Dt;
            StartTime=FMath::Min(Entry,LaunchTime+30.);
            Late=Entry>LaunchTime+30.; State=EState::Racing; ++Signals;
        }
        else if(Now>=LaunchTime+30.) { Late=true; StartTime=LaunchTime+30.; }
        return;
    }
    if(State==EState::Racing)
    {
        const bool ToB=(Legs%2)==0;
        const FVector Origin=ToB?C.B:C.A, Normal=ToB?C.Axis():-C.Axis();
        const bool Crossed=Crossing(Origin,Normal,F);
        const bool Outside=Extent(After,Origin,Normal)>=0.;
        const bool Delayed=PendingTurn;
        if(!Outside) PendingTurn=false;
        if(Crossed && !JudgeVisible) PendingTurn=true;
        if(JudgeVisible && (Crossed || (PendingTurn && Outside)))
        {
            if(Delayed) F=1.;
            PendingTurn=false;
            ++Legs; ++Signals;
            const double Stamp=PreviousTime+F*Dt;
            Splits.Add(Stamp-StartTime);
            if(Legs==10) { FinishTime=Stamp; State=EState::Clearance; }
        }
    }
    if(State==EState::Clearance)
    {
        // F3F 1.6 h: some part must reach launch-area height within five seconds.
        double ReachedAt=DBL_MAX;
        for(int32 I=0;I<After.Num();++I)
        {
            if(Before[I].Z>=LaunchZ) ReachedAt=FMath::Min(ReachedAt,PreviousTime);
            else if(After[I].Z>=LaunchZ)
                ReachedAt=FMath::Min(ReachedAt,PreviousTime+Dt*(LaunchZ-Before[I].Z)/(After[I].Z-Before[I].Z));
        }
        if(ReachedAt<=FinishTime+5.) State=EState::Finished;
        else if(Now-FinishTime>5.) { Reason=TEXT("Hauteur de départ non rejointe sous 5 s"); State=EState::Invalid; }
    }
}
