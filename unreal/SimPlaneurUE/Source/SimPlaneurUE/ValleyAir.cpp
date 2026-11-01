#include "ValleyAir.h"
#include "SlopeWorld.h"
#include "GliderDynamics.h"
#include "ValleyLandscape.h"

double ValleyAir::SurfaceAltitude(double North, double East)
{
    return .01 * FMath::Max(ASlopeWorld::TerrainHeightCm(100 * North, 100 * East), Valley::WaterLevel);
}

FVector ValleyAir::TerrainFlow(const FVector2D& XY, double HeightAGL, const FVector2D& Breeze)
{
    const double Speed = Breeze.Size();
    if (Speed < UE_SMALL_NUMBER) return FVector::ZeroVector;
    const FVector2D Direction = Breeze / Speed;
    const double Height = FMath::Max(0., HeightAGL);
    auto GradeAt=[&](double Radius)
    {
        const FVector2D Forward=XY+Radius*Direction, Back=XY-Radius*Direction;
        return (SurfaceAltitude(Forward.X,Forward.Y)-SurfaceAltitude(Back.X,Back.Y))/(2.*Radius);
    };
    // Small bumps vanish first with altitude, while the whole hillside still
    // displaces air above and ahead of it. Recover the tangent at the surface.
    const double Medium=GradeAt(12.+.35*Height)*FMath::Exp(-Height/180.);
    const double Broad=GradeAt(45.+.65*Height)*FMath::Exp(-Height/380.);
    const double Grade=FMath::Lerp(.65*Medium+.35*Broad,GradeAt(1.),FMath::Exp(-Height/8.));
    // Turn the incident velocity without increasing its magnitude on steep faces.
    return FVector(Breeze.X, Breeze.Y, -Speed * Grade) / FMath::Sqrt(1. + Grade * Grade);
}

FVector ValleyAir::Velocity(const FVector& Position, double Seconds, double* RidgeLift)
{
    const FAirSample Air=Sample(Position,Seconds,Settings());
    if (RidgeLift) *RidgeLift=Air.RidgeLiftMps;
    return Air.VelocityNED;
}
double ValleyAir::SampleGlider(FGliderDynamics& Glider, double Seconds)
{
    const FVector Centre = Glider.PositionNED();
    const FVector Offset = Glider.RightTipOffsetNED();
    const FVector Right = Velocity(Centre + Offset, Seconds);
    const FVector Left = Velocity(Centre - Offset, Seconds);
    double RidgeLift;
    Glider.SetWind(Velocity(Centre, Seconds, &RidgeLift), Right - Left);
    return RidgeLift;
}

double ValleyAir::FromBearing(const FVector& Flow)
{
    return FRotator::ClampAxis(FMath::RadiansToDegrees(FMath::Atan2(-Flow.Y, -Flow.X)));
}

FVector2D ValleyAir::CompassDirection(double Bearing, double CameraYaw)
{
    const double Angle = FMath::DegreesToRadians(Bearing - CameraYaw);
    return FVector2D(FMath::Sin(Angle), -FMath::Cos(Angle));
}
