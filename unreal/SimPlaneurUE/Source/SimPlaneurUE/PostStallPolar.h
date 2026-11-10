#pragma once
#include "CoreMinimal.h"
#include "Algo/BinarySearch.h"

// Static, approximate full-angle extension; this does not model dynamic stall
// or establish a spin equilibrium. Keep every supplied polar sample unchanged.
// The outer law uses the flat-plate terms also present in AeroDyn's Viterna
// extension (Appendix D); the smooth endpoint bridge below is our approximation,
// not the complete Viterna method:
// https://www2.nrel.gov/docs/libraries/wind-docs/aerodyn.pdf?sfvrsn=86abc14f_1
// Cd90=1.28 is only a generic plate estimate, not a measured aircraft value:
// https://www1.grc.nasa.gov/beginners-guide-to-aeronautics/shape-effects-on-drag/
namespace PostStallPolar
{
    inline int32 Interval(const TArray<double>& Values,double Value,double& Fraction)
    {
        const int32 I=FMath::Clamp(Algo::UpperBound(Values,Value)-1,0,Values.Num()-2);
        Fraction=FMath::Clamp((Value-Values[I])/(Values[I+1]-Values[I]),0.,1.);
        return I;
    }

    // Angles are radians, lift has one entry per angle, and drag is flattened
    // [angle][speed]. Caller supplies finite, increasing axes as LoadModel does.
    // Speed remains clamped to the measured speed range, including zero speed.
    inline void Sample(const TArray<double>& Angles,const TArray<double>& Lift,
        const TArray<double>& Drag,const TArray<double>& Speeds,double Alpha,double Speed,
        double& OutCL,double& OutCD,double Cd90=1.28)
    {
        OutCL=OutCD=0.;
        if (Angles.Num()<2 || Speeds.Num()<2 || Lift.Num()!=Angles.Num() ||
            Drag.Num()!=Angles.Num()*Speeds.Num() || !FMath::IsFinite(Alpha) || !FMath::IsFinite(Speed)) return;
        // The coefficients and angles are double precision. UE's legacy PI is
        // a float constant; using it here creates a seam at reverse flow.
        if (Alpha < -UE_DOUBLE_PI || Alpha > UE_DOUBLE_PI)
        {
            Alpha=FMath::Fmod(Alpha+UE_DOUBLE_PI,2.*UE_DOUBLE_PI);
            if (Alpha<0.) Alpha+=2.*UE_DOUBLE_PI;
            Alpha-=UE_DOUBLE_PI;
        }
        double SpeedFraction;
        const int32 Column=Interval(Speeds,Speed,SpeedFraction);
        const auto DragAt=[&](int32 Row) {
            return FMath::Lerp(Drag[Row*Speeds.Num()+Column],Drag[Row*Speeds.Num()+Column+1],SpeedFraction);
        };
        if (Alpha>=Angles[0] && Alpha<=Angles.Last())
        {
            double AngleFraction;
            const int32 Row=Interval(Angles,Alpha,AngleFraction);
            OutCL=FMath::Lerp(Lift[Row],Lift[Row+1],AngleFraction);
            OutCD=FMath::Lerp(DragAt(Row),DragAt(Row+1),AngleFraction);
            return;
        }

        Cd90=FMath::IsFinite(Cd90) && Cd90>0. ? Cd90 : 1.28;
        double EdgeDrag=DragAt(0);
        for (int32 I=1;I<Angles.Num();++I) EdgeDrag=FMath::Min(EdgeDrag,DragAt(I));
        EdgeDrag=FMath::Max(0.,EdgeDrag);
        const double Sign=Alpha<Angles[0] ? -1. : 1.;
        const int32 End=Sign<0. ? 0 : Angles.Num()-1, Adjacent=Sign<0. ? 1 : End-1;
        const double Match=Sign*Angles[End], Angle=Sign*Alpha, Distance=Angle-Match;
        const double Step=Sign*(Angles[End]-Angles[Adjacent]);
        const double LiftSlope=Sign*(Lift[End]-Lift[Adjacent])/Step;
        const double DragSlope=(DragAt(End)-DragAt(Adjacent))/Step;
        const double Limit=Match<.5*UE_DOUBLE_PI ? .5*UE_DOUBLE_PI : UE_DOUBLE_PI;
        const double Width=FMath::Min(FMath::DegreesToRadians(20.),FMath::Max(1.e-6,Limit-Match));
        const double T=FMath::Clamp(Distance/Width,0.,1.);
        const double Blend=T*T*(3.-2.*T);
        const double Sin=FMath::Sin(Angle), Cos=FMath::Cos(Angle);
        const double PlateCL=Cd90*Sin*Cos;
        const double PlateCD=Cd90*Sin*Sin+EdgeDrag*Cos*Cos;
        // The tangent continuation and zero endpoint blend derivatives retain
        // the supplied final-segment slope. Outside the bridge use the periodic
        // plate law directly, with nonnegative drag and zero lift at 90/180 deg.
        OutCL=Sign*FMath::Lerp(Sign*Lift[End]+LiftSlope*Distance,PlateCL,Blend);
        OutCD=FMath::Max(0.,FMath::Lerp(DragAt(End)+DragSlope*Distance,PlateCD,Blend));
    }
}
