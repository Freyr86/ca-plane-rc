#include "ValleyAir.h"
#include "ValleyLandscape.h"
#include "Misc/ConfigCacheIni.h"

namespace
{
    ValleyAir::FWeatherSettings CurrentWeather;
    constexpr double Cell=450.;
    constexpr int32 HalfGrid=16, GridWidth=2*HalfGrid+1;
    struct FThermal
    {
        FVector2D Source;
        double Ground, Radius, Strength, Phase;
        FThermal(double North,double East,double CoreRadius,double PeakLift,double PulsePhase)
            : Source(North,East),Ground(ValleyAir::SurfaceAltitude(North,East)),
              Radius(CoreRadius),Strength(PeakLift),Phase(PulsePhase)
        {
            if (Ground<=Valley::WaterLevel*.01+2.) Strength=0.;
            else Strength*=1.-.35*Valley::Woodland(North*100.,East*100.);
        }
    };

    const TArray<FThermal>& ThermalGrid()
    {
        // World-fixed solar sources, not spawned around the camera or plane.
        // Cache the terrain outside the 120 Hz sampling path.
        static const TArray<FThermal> Grid=[]
        {
            TArray<FThermal> Result;
            Result.Reserve(GridWidth*GridWidth);
            for (int32 X=-HalfGrid;X<=HalfGrid;++X)
                for (int32 Y=-HalfGrid;Y<=HalfGrid;++Y)
                {
                    FRandomStream R(HashCombineFast(GetTypeHash(FIntPoint(X,Y)),36004522u));
                    const double N=(X+R.FRandRange(.2f,.8f))*Cell;
                    const double E=(Y+R.FRandRange(.2f,.8f))*Cell;
                    Result.Emplace(N,E,R.FRandRange(30.f,42.f),R.FRandRange(2.5f,3.6f),R.FRandRange(0.f,2*PI));
                }
            return Result;
        }();
        return Grid;
    }

    double ThermalLift(const FThermal& Source,const FVector2D& XY,double Altitude,
                       double Time,const FVector2D& FlowTo,const ValleyAir::FWeatherSettings& Weather)
    {
        const double Height=Altitude-Source.Ground;
        if (Source.Strength<=0. || Height<=0. || Height>=650.) return 0.;
        // Bent-plume approximation: downwind displacement follows wind/ascent
        // speed, then saturates as entrainment mixes the upper plume.
        const double Travel=Weather.WindSpeedMps*Height/FMath::Max(2.,Source.Strength*1.4);
        const FVector2D Centre=Source.Source+FlowTo*(240.*(1.-FMath::Exp(-Travel/240.)));
        const double Radius=Source.Radius+.085*Height;
        const double R2=(XY-Centre).SizeSquared()/FMath::Square(Radius);
        if (R2>=36.) return 0.;
        // Broad useful core, weak compensation over nine times its area.
        // The untruncated radial profile has zero net vertical mass flux.
        const double Profile=(FMath::Exp(-R2)-FMath::Exp(-R2/9.)/9.)/(8./9.);
        const double Edge=1.-FMath::SmoothStep(25.,36.,R2);
        const double Cycle=.65+.35*FMath::Sin(Time*.021+Source.Phase);
        const double Depth=FMath::SmoothStep(0.,18.,Height)*(1.-FMath::SmoothStep(450.,650.,Height));
        return Weather.ThermalStrength*Source.Strength*Cycle*Depth*Profile*Edge;
    }
}

FVector ValleyAir::SunDirection(int32 SolarMinutes)
{
    // Representative alpine summer day, latitude 46 N, solar declination +20 degrees.
    const double HourAngle=FMath::DegreesToRadians((SolarMinutes/60.-12.)*15.);
    constexpr double Latitude=46.*UE_PI/180., Declination=20.*UE_PI/180.;
    const double Meridian=FMath::Cos(Declination)*FMath::Cos(HourAngle);
    return FVector(FMath::Cos(Latitude)*FMath::Sin(Declination)-FMath::Sin(Latitude)*Meridian,
        -FMath::Cos(Declination)*FMath::Sin(HourAngle),
        FMath::Sin(Latitude)*FMath::Sin(Declination)+FMath::Cos(Latitude)*Meridian);
}

double ValleyAir::SolarHeating(const FWeatherSettings& Weather)
{
    constexpr double Transmission[]={1.12,1.,.24,.45};
    static const double Reference=SunDirection(14*60).Z;
    return FMath::Max(0.,SunDirection(Weather.SolarMinutes).Z)/Reference
        *Transmission[FMath::Clamp(Weather.Sky,0,3)];
}

ValleyAir::FWeatherSettings ValleyAir::Settings() { return CurrentWeather; }

void ValleyAir::SetSettings(const FWeatherSettings& Weather)
{
    CurrentWeather.SolarMinutes=FMath::Clamp(Weather.SolarMinutes,0,1439);
    CurrentWeather.Sky=FMath::Clamp(Weather.Sky,0,3);
    const FWeatherSettings Default;
    CurrentWeather.WindSpeedMps=FMath::IsFinite(Weather.WindSpeedMps) ? FMath::Clamp(Weather.WindSpeedMps,0.,10.) : Default.WindSpeedMps;
    CurrentWeather.WindFromDegrees=FMath::IsFinite(Weather.WindFromDegrees) ? FRotator::ClampAxis(Weather.WindFromDegrees) : Default.WindFromDegrees;
    CurrentWeather.ThermalStrength=FMath::IsFinite(Weather.ThermalStrength) ? FMath::Clamp(Weather.ThermalStrength,0.,2.) : Default.ThermalStrength;
    CurrentWeather.Turbulence=FMath::IsFinite(Weather.Turbulence) ? FMath::Clamp(Weather.Turbulence,0.,1.) : Default.Turbulence;
}

void ValleyAir::LoadSettings()
{
    FWeatherSettings Weather;
    GConfig->GetInt(TEXT("SimPlaneur.Weather"),TEXT("SolarMinutes"),Weather.SolarMinutes,GGameUserSettingsIni);
    GConfig->GetInt(TEXT("SimPlaneur.Weather"),TEXT("Sky"),Weather.Sky,GGameUserSettingsIni);
    GConfig->GetDouble(TEXT("SimPlaneur.Weather"),TEXT("WindSpeedMps"),Weather.WindSpeedMps,GGameUserSettingsIni);
    GConfig->GetDouble(TEXT("SimPlaneur.Weather"),TEXT("WindFromDegrees"),Weather.WindFromDegrees,GGameUserSettingsIni);
    GConfig->GetDouble(TEXT("SimPlaneur.Weather"),TEXT("ThermalStrength"),Weather.ThermalStrength,GGameUserSettingsIni);
    GConfig->GetDouble(TEXT("SimPlaneur.Weather"),TEXT("Turbulence"),Weather.Turbulence,GGameUserSettingsIni);
    SetSettings(Weather);
}

void ValleyAir::SaveSettings()
{
    GConfig->SetInt(TEXT("SimPlaneur.Weather"),TEXT("SolarMinutes"),CurrentWeather.SolarMinutes,GGameUserSettingsIni);
    GConfig->SetInt(TEXT("SimPlaneur.Weather"),TEXT("Sky"),CurrentWeather.Sky,GGameUserSettingsIni);
    GConfig->SetDouble(TEXT("SimPlaneur.Weather"),TEXT("WindSpeedMps"),CurrentWeather.WindSpeedMps,GGameUserSettingsIni);
    GConfig->SetDouble(TEXT("SimPlaneur.Weather"),TEXT("WindFromDegrees"),CurrentWeather.WindFromDegrees,GGameUserSettingsIni);
    GConfig->SetDouble(TEXT("SimPlaneur.Weather"),TEXT("ThermalStrength"),CurrentWeather.ThermalStrength,GGameUserSettingsIni);
    GConfig->SetDouble(TEXT("SimPlaneur.Weather"),TEXT("Turbulence"),CurrentWeather.Turbulence,GGameUserSettingsIni);
    GConfig->Flush(false,GGameUserSettingsIni);
}

ValleyAir::FAirSample ValleyAir::Sample(const FVector& Position,double Seconds,const FWeatherSettings& Weather)
{
    FAirSample Result;
    const FVector2D XY(Position.X,Position.Y);
    const double Altitude=-Position.Z;
    const double AGL=FMath::Max(0.,Altitude-SurfaceAltitude(XY.X,XY.Y));
    // Logarithmic surface layer normalized at 10 m, capped above 120 m.
    const double Shear=FMath::Loge((FMath::Min(AGL,120.)+.08)/.08)/FMath::Loge(10.08/.08);
    const double Bearing=FMath::DegreesToRadians(Weather.WindFromDegrees);
    const FVector2D FlowTo(-FMath::Cos(Bearing),-FMath::Sin(Bearing));
    Result.VelocityNED=TerrainFlow(XY,AGL,FlowTo*(Weather.WindSpeedMps*Shear));
    Result.RidgeLiftMps=-Result.VelocityNED.Z;
    if (Weather.ThermalStrength>0.)
    {
        static const FThermal LocalSources[]={{65,10,35,3.1,.8},{145,90,42,3.4,1.8},{-48,84,36,2.9,3.7}};
        for (const auto& Source : LocalSources)
            Result.ThermalLiftMps+=ThermalLift(Source,XY,Altitude,Seconds,FlowTo,Weather);
        const int32 CentreX=FMath::FloorToInt(XY.X/Cell), CentreY=FMath::FloorToInt(XY.Y/Cell);
        const auto& Grid=ThermalGrid();
        // Max plume shift 240 m + max radial support 584 m < two cells (900 m).
        for (int32 X=FMath::Max(-HalfGrid,CentreX-2);X<=FMath::Min(HalfGrid,CentreX+2);++X)
            for (int32 Y=FMath::Max(-HalfGrid,CentreY-2);Y<=FMath::Min(HalfGrid,CentreY+2);++Y)
            {
                const auto& Source=Grid[(X+HalfGrid)*GridWidth+Y+HalfGrid];
                Result.ThermalLiftMps+=ThermalLift(Source,XY,Altitude,Seconds,FlowTo,Weather);
            }
        Result.ThermalLiftMps*=SolarHeating(Weather)*FMath::SmoothStep(0.,7.,AGL);
        Result.VelocityNED.Z-=Result.ThermalLiftMps;
    }
    // Smooth advected eddies, shared by the wings, instruments and air trails.
    // Low-level and lee turbulence is stronger; no random per-frame impulses.
    const FVector2D Advected=XY-FlowTo*(Weather.WindSpeedMps*Seconds);
    const double E1=FMath::Sin(Advected.X*.023+Advected.Y*.017+Seconds*.11+AGL*.013);
    const double E2=FMath::Sin(Advected.X*.057-Advected.Y*.033-Seconds*.17+AGL*.031);
    const double E3=FMath::Sin(Advected.Y*.041-Seconds*.09+AGL*.047);
    const double Lee=FMath::Clamp(-Result.RidgeLiftMps/FMath::Max(.5,Weather.WindSpeedMps),0.,1.);
    const double Amplitude=Weather.Turbulence*Weather.WindSpeedMps*(.18+.35*FMath::Exp(-AGL/65.)+.45*Lee)*FMath::SmoothStep(0.,5.,AGL);
    Result.TurbulenceNED=Amplitude*FVector(.7*E1+.3*E2,.7*E3-.3*E1,.55*E2+.2*E3);
    Result.VelocityNED+=Result.TurbulenceNED;
    return Result;
}
