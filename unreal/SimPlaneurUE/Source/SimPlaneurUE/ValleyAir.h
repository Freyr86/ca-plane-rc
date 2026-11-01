#pragma once

#include "CoreMinimal.h"

struct FGliderDynamics;

// SI, world north/east/down. All consumers sample the same time and coordinates.
namespace ValleyAir
{
    struct FWeatherSettings
    {
        double WindSpeedMps = 4.5; // Reference wind at 10 m AGL, m/s.
        double WindFromDegrees = 12.;
        double ThermalStrength = 1.;
        double Turbulence = .15;
        int32 SolarMinutes = 14*60;
        int32 Sky = 1; // Clear, broken clouds, overcast, mist.

    };
    struct FAirSample
    {
        FVector VelocityNED = FVector::ZeroVector;
        double RidgeLiftMps = 0.;
        double ThermalLiftMps = 0.;
        FVector TurbulenceNED = FVector::ZeroVector;
    };
    FVector SunDirection(int32 SolarMinutes); // Unit vector towards sun, Unreal north/east/up.
    double SolarHeating(const FWeatherSettings& Weather);
    FWeatherSettings Settings();
    void SetSettings(const FWeatherSettings& Weather);
    void LoadSettings();
    void SaveSettings();
    FAirSample Sample(const FVector& PositionNED, double Seconds, const FWeatherSettings& Weather);
    FVector TerrainFlow(const FVector2D& XY, double HeightAGL, const FVector2D& Breeze);
    FVector Velocity(const FVector& PositionNED, double Seconds, double* RidgeLift = nullptr);
    double SampleGlider(FGliderDynamics& Glider, double Seconds);
    double SurfaceAltitude(double North, double East);

    // Bearing of the origin of a horizontal flow, clockwise from world north.
    double FromBearing(const FVector& FlowNED);
    FVector2D CompassDirection(double Bearing, double CameraYaw);
}
