#pragma once
#include "CoreMinimal.h"

// Mass-matched electric winch, SI units. The reel slows under load; an elastic
// towline transmits its pull. No aircraft position, speed or attitude is imposed.
struct FWinchDrive
{
    double Mass = 1., PaidOut = 150., InitialLength = 150.;
    double ReelSpeed = 0., Tension = 0., Elapsed = 0., SlackSeconds = 0.;
    double Power = 1.;
    bool WasTaut = false;
    static constexpr double MinLayout = 40., MaxLayout = 400., EndLength = 12.;
    double MaximumForce() const { return Mass * 9.80665 * 5.5; }
    void Start(double Length, double MassKg)
    {
        *this = FWinchDrive();
        Mass = FMath::Max(.05,MassKg);
        InitialLength = PaidOut = FMath::Max(EndLength,Length);
    }
    void Step(double Dt, double Distance, double RadialSpeed, double Command)
    {
        Elapsed += Dt;
        Power = FMath::Clamp(Command,0.,1.5);
        const double Ramp = FMath::Clamp(Elapsed / .9,0.,1.);
        const double Stall = MaximumForce()*Power;
        const double TargetSpeed = 32.*FMath::Sqrt(Power)*Ramp
            * FMath::Clamp(1.-Tension/FMath::Max(.01,Stall),0.,1.);
        ReelSpeed += FMath::Clamp(TargetSpeed-ReelSpeed,-35.*Dt,22.*Dt);
        PaidOut = FMath::Max(EndLength,PaidOut-ReelSpeed*Dt);
        const double Extension = Distance-PaidOut;
        const double Spring = Mass*9.80665*8.*FMath::Clamp(150./InitialLength,.4,2.);
        const double Damping = .8*FMath::Sqrt(Spring*Mass);
        Tension = Extension > 0. ? FMath::Clamp(Spring*Extension+Damping*(RadialSpeed+ReelSpeed),0.,MaximumForce()*1.35) : 0.;
        WasTaut |= Tension > Mass*9.80665;
        SlackSeconds = Tension < Mass*.5 ? SlackSeconds+Dt : 0.;
    }
    bool Slack() const { return WasTaut && Elapsed>1.5 && SlackSeconds>.35; }
    bool Finished() const { return PaidOut<=EndLength+.01 || Elapsed>40.; }
};
