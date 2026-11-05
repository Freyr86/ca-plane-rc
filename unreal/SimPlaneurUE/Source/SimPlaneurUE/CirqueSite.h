#pragma once
#include "CoreMinimal.h"

namespace CirqueSite
{
    // Unreal centimetres: north is +X, east is +Y.
    // Placed on the plateau, four metres behind the existing main wall opening.
    extern const FVector2D Pilot,Forward,Right;
    extern const float Heading;
    inline FVector2D Point(double Ahead,double Side) { return Pilot+Forward*Ahead+Right*Side; }
    extern const FVector2D BenchLeft,BenchRight,Table,Sock;
    inline bool Clearing(float X,float Y) { return FVector2D::DistSquared({X,Y},Pilot)<FMath::Square(1400.); }
    inline bool ShortGrass(float X,float Y) { return FVector2D::DistSquared({X,Y},Pilot)<FMath::Square(1100.); }
    bool FurnitureFootprint(float X,float Y);
    FVector Ground(float X,float Y);
}
