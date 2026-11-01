#include "ValleyAir.h"
#include "SlopeWorld.h"
#include "GliderDynamics.h"
#include "ValleyLandscape.h"

namespace
{
    struct FThermal
    {
        FVector2D Source;
        double Ground;
        double Radius;
        double Strength;
        double Phase;

        FThermal(double North, double East, double CoreRadius, double PeakLift, double PulsePhase)
            : Source(North, East), Ground(ValleyAir::SurfaceAltitude(North, East)),
              Radius(CoreRadius), Strength(PeakLift), Phase(PulsePhase) {}
    };
}

double ValleyAir::SurfaceAltitude(double North, double East)
{
    return .01 * FMath::Max(ASlopeWorld::TerrainHeightCm(100 * North, 100 * East), Valley::WaterLevel);
}

FVector ValleyAir::TerrainFlow(const FVector2D& XY, double HeightAGL, const FVector2D& Breeze)
{
    const double Speed = Breeze.Size();
    if (Speed < UE_SMALL_NUMBER) return FVector::ZeroVector;
    const FVector2D Direction = Breeze / Speed;
    // At the surface the wind follows the local tangent. Aloft it responds
    // to a broader footprint of terrain, with progressively less deflection.
    const double Height = FMath::Max(0., HeightAGL);
    const double Radius = FMath::Min(100., .75 + .60 * Height);
    const FVector2D Along = XY + Radius * Direction;
    const FVector2D Against = XY - Radius * Direction;
    const double Rise = SurfaceAltitude(Along.X, Along.Y) - SurfaceAltitude(Against.X, Against.Y);
    const double Grade = Rise / (2. * Radius) * FMath::Exp(-Height / 90.);
    // Turn the incident velocity without increasing its magnitude on steep faces.
    return FVector(Breeze.X, Breeze.Y, -Speed * Grade) / FMath::Sqrt(1. + Grade * Grade);
}

FVector ValleyAir::Velocity(const FVector& Position, double Seconds, double* RidgeLift)
{
    const FVector2D XY(Position.X, Position.Y);
    const double Altitude = -Position.Z;
    const double AGL = FMath::Max(0., Altitude - SurfaceAltitude(XY.X, XY.Y));
    // Clear day. A light northerly breeze veers and strengthens gently above the grass.
    const double Gust = FMath::Sin(Seconds * .16 + XY.Y * .013 - XY.X * .009);
    const double Veer = FMath::Sin(Seconds * .083 + XY.X * .011);
    const double Speed = (2.1 + .34 * Gust) * (.62 + .38 * (1. - FMath::Exp(-AGL / 14.)));
    const double From = FMath::DegreesToRadians(12. + 8. * Veer);
    const FVector2D FlowTo(-FMath::Cos(From), -FMath::Sin(From));

    // Sun-warmed ground sources. Rising air leans downwind and broadens with height.
    static const FThermal Sources[] = {{65, 10, 14, 2.15, .3}, {145, 90, 22, 2.7, 1.8}, {-48, 84, 18, 2.1, 3.7}};
    const FVector2D MeanFlowTo(-.978148, -.207912);
    double Lift = 0.;
    for (const FThermal& Thermal : Sources)
    {
        const double Height = Altitude - Thermal.Ground;
        if (Height <= 0. || Height >= 230.) continue;
        const FVector2D Centre = Thermal.Source + MeanFlowTo * (.54 * Height);
        const double Radius = Thermal.Radius + .09 * Height;
        const double R2 = (XY - Centre).SizeSquared() / FMath::Square(Radius);
        if (R2 > 14.) continue;
        const double Profile = (1. - R2) * FMath::Exp(-R2); // rising core, sinking annulus
        const double Pulse = .9 + .1 * FMath::Sin(Seconds * .054 + Thermal.Phase);
        const double Depth = FMath::SmoothStep(0., 10., Height) * (1. - FMath::SmoothStep(165., 230., Height));
        Lift += Thermal.Strength * Pulse * Profile * Depth;
    }
    Lift *= FMath::SmoothStep(0., 7., AGL);
    FVector Air = TerrainFlow(XY, AGL, FlowTo * Speed);
    if (RidgeLift) *RidgeLift = -Air.Z;
    Air.Z -= Lift;
    return Air;
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
