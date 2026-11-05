#include "CirqueLandscape.h"

namespace
{
    struct FSurface
    {
        Cirque::FSample Masks;
        float Height = 0.f;
    };

    float Noise(const FVector2D& P, float Scale, const FVector2D& Offset)
    {
        return FMath::PerlinNoise2D(P / Scale + Offset);
    }

    struct FRimProfile
    {
        FVector2D Point;
        float NorthBay,SouthBay,Buttress,Lip,Enclosure,Width,Cornice;
    };

    FRimProfile RimProfile(float Angle,float Facing)
    {
        FRimProfile S;
        S.Point={Cirque::RimRadius*FMath::Cos(Angle),Cirque::RimRadius*FMath::Sin(Angle)};
        const FVector2D Folded(.65f*S.Point.X,1.3f*S.Point.Y);
        const float BroadFold=Noise(Folded,23000.f,{4.1,9.3});
        const float Fissures=Noise({.42f*S.Point.X,1.1f*S.Point.Y},6500.f,{7.4,2.1});
        S.NorthBay=FMath::Exp(-FMath::Square((Angle-.47f)/.16f));
        S.SouthBay=FMath::Exp(-FMath::Square((Angle+1.32f)/.13f));
        S.Buttress=FMath::Exp(-FMath::Square((Angle+.72f)/.12f));
        S.Lip=Cirque::RimRadius+6200.f*BroadFold+2600.f*Fissures
            +2700.f*FMath::Sin(3.f*Angle+.45f)+1400.f*FMath::Sin(8.f*Angle-1.2f)
            +5200.f*S.NorthBay+3600.f*S.SouthBay-4200.f*S.Buttress;
        S.Enclosure=FMath::SmoothStep(-.84f,-.20f,Facing);
        S.Width=2500.f+500.f*Noise(S.Point,18000.f,{8.2,2.6})+9500.f*FMath::Square(1.f-S.Enclosure);
        S.Cornice=320.f*Noise(S.Point,2200.f,{2.6,7.1});
        return S;
    }

    FSurface Surface(float X, float Y)
    {
        FSurface Result;
        const FVector2D P(X - Cirque::CenterX, Y - Cirque::CenterY);
        const float RadiusSquared = P.SizeSquared();
        if (RadiusSquared >= FMath::Square(Cirque::TransitionRadius)) return Result;

        const float R = FMath::Sqrt(RadiusSquared);
        const float U = P.X * Cirque::BackAxisX + P.Y * Cirque::BackAxisY;
        const float V = -P.X * Cirque::BackAxisY + P.Y * Cirque::BackAxisX;
        const float Angle = FMath::Atan2(V, U);

        // The envelope has an irregular edge and a zero derivative at both ends.
        // Keeping its end strictly below TransitionRadius also preserves the old
        // terrain and its derivatives exactly at the advertised patch boundary.
        const float BlendStart = 77000.f + 4500.f * Noise(P, 47000.f, {7.2, 3.8});
        const float BlendEnd = 106000.f + 2500.f * Noise(P, 56000.f, {1.9, 8.6});
        const float Influence = 1.f - FMath::SmoothStep(BlendStart, BlendEnd, R);
        if (Influence <= 0.f) return Result;

        // Sample on the rim, rather than across its thickness: folded buttresses
        // and narrow ravines continue vertically instead of becoming wavy dunes.
        const float Facing = R > 1.f ? U / R : 1.f;
        const FRimProfile Profile=RimProfile(Angle,Facing);
        const FVector2D RimPoint=Profile.Point;
        const float NorthBay=Profile.NorthBay,SouthBay=Profile.SouthBay,Buttress=Profile.Buttress;
        const float Lip=Profile.Lip,Enclosure=Profile.Enclosure,WallWidth=Profile.Width;
        const float WallStart = Lip - .5f * WallWidth;
        const float WallEnd = Lip + .5f * WallWidth;
        float WallT = (R - WallStart) / WallWidth;
        const float Cornice = Profile.Cornice;
        WallT += (Cornice / WallWidth) * FMath::SmoothStep(.65f, .88f, WallT)
            * (1.f - FMath::SmoothStep(1.f, 1.25f, WallT));
        const float BedShift = .045f * Noise(RimPoint, 10500.f, {6.1, 3.8});
        // Unequal steep faces separated by short, broken bedding ledges. Their
        // positions vary along the face; no ledge extends into the pasture.
        const float Wall = .46f * FMath::SmoothStep(0.f, .34f + BedShift, WallT)
            + .10f * FMath::SmoothStep(.34f + BedShift, .55f + BedShift, WallT)
            + .32f * FMath::SmoothStep(.51f + BedShift, .82f, WallT)
            + .12f * FMath::SmoothStep(.82f, 1.f, WallT);

        // Individual scree fans broaden below the ravines and narrow underneath
        // projecting buttresses. Small lobes feather into the basin vegetation.
        const float FanNoise = Noise(RimPoint, 12500.f, {5.7, 6.2});
        const float TalusDepth = FMath::Clamp(14000.f + 6500.f * FanNoise
            + 6500.f * NorthBay + 5000.f * SouthBay - 4000.f * Buttress, 8000.f, 22000.f);
        const float Toe = Lip - TalusDepth - 1500.f * Noise(P, 5500.f, {3.8, 1.6});
        const float TalusRun = FMath::SmoothStep(Toe, WallStart - 700.f, R);
        const float TalusHeight = 2800.f + 1000.f * Noise(P, 12000.f, {2.3, 5.8});
        const float Talus = TalusHeight * FMath::Pow(TalusRun, 1.6f)
            + 230.f * Noise(P, 1800.f, {4.8, 2.5}) * TalusRun;

        // The floor drains gently towards the open combe. The surrounding crown
        // stays rolling: its broad height variation survives above the rock face.
        const float Floor = 11100.f + .012f * U + .00000008f * V * V
            + 350.f * Noise(P, 27000.f, {3.4, 7.6});
        const float CrownFold = 3600.f * Noise(RimPoint, 24000.f, {8.4, 4.3})
            + 1700.f * Noise(RimPoint, 8000.f, {1.6, 2.9})
            + 1900.f * FMath::Sin(2.f * Angle + .8f);
        const float RimCrown = 30600.f + 2200.f * CrownFold
            / FMath::Sqrt(FMath::Square(2200.f) + CrownFold * CrownFold);
        // Farther out, return to broad rolling pasture so the angular rim shape
        // does not draw radial grooves across the surrounding plateau.
        const float MeadowCrown = 30600.f + 1500.f * Noise(P, 42000.f, {8.4, 4.3})
            + 550.f * Noise(P, 14500.f, {1.6, 2.9});
        const float Crown = FMath::Lerp(RimCrown, MeadowCrown,
            FMath::SmoothStep(Lip + 2500.f, Lip + 14000.f, R));
        // Non-periodic metre-scale breaks, resolved by the one-metre wall mesh.
        // Keep them off the rim and talus: no fine repeated bedding modulation
        // may alias against the physical tessellation or its analytic normals.
        const float Roughness = 230.f * Noise(P, 750.f, {9.2, 3.1})
            + 90.f * Noise(P, 470.f, {4.7, 8.3});
        const float FaceEnvelope = FMath::SmoothStep(.04f, .18f, Wall)
            * (1.f - FMath::SmoothStep(.82f, .98f, Wall));
        Result.Height = Floor + Enclosure * (FMath::Lerp(Talus, Crown - Floor, Wall)
            + Roughness * FaceEnvelope);

        const float Cliff = Enclosure
            * FMath::SmoothStep(WallStart - 700.f, WallStart + 450.f, R)
            * (1.f - FMath::SmoothStep(WallEnd - 450.f, WallEnd + 700.f, R));
        const float Scree = Enclosure
            * FMath::SmoothStep(Toe - 1800.f, Toe + 2500.f, R)
            * (1.f - FMath::SmoothStep(WallStart - 1100.f, WallStart + 600.f, R));
        Result.Masks.Influence = Influence;
        Result.Masks.Cliff = Influence * Cliff;
        Result.Masks.Scree = Influence * Scree;
        Result.Masks.Meadow = Influence * (1.f - Cliff) * (1.f - Scree);
        return Result;
    }
}

Cirque::FSample Cirque::Sample(float X, float Y)
{
    return Surface(X, Y).Masks;
}

float Cirque::ApplyHeight(float X, float Y, float BaseHeight)
{
    const FSurface S = Surface(X, Y);
    if (S.Masks.Influence <= 0.f) return BaseHeight;
    return FMath::Lerp(BaseHeight, S.Height, S.Masks.Influence);
}

FVector2D Cirque::RimEdge(float Angle)
{
    const FRimProfile S=RimProfile(Angle,FMath::Cos(Angle));
    // The cornice displaces WallT: solve its exact upper endpoint rather than
    // using a rock-material threshold several metres farther onto the grass.
    float Low=.5f,High=1.5f;
    for(int32 I=0;I<22;++I)
    {
        const float T=(Low+High)*.5f;
        const float WallT=T+(S.Cornice/S.Width)*FMath::SmoothStep(.65f,.88f,T)
            *(1.f-FMath::SmoothStep(1.f,1.25f,T));
        if(WallT<1.f) Low=T; else High=T;
    }
    const float Radius=S.Lip-.5f*S.Width+S.Width*(Low+High)*.5f;
    const FVector2D Back(BackAxisX,BackAxisY),Side(-BackAxisY,BackAxisX);
    return FVector2D(CenterX,CenterY)+(Back*FMath::Cos(Angle)+Side*FMath::Sin(Angle))*Radius;
}
