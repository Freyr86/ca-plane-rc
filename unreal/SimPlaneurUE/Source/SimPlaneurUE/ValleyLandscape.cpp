#include "ValleyLandscape.h"

namespace
{
    float Noise(FVector2D P, float Scale, FVector2D Offset)
    {
        return FMath::PerlinNoise2D(P / Scale + Offset);
    }

    float Bell(float X)
    {
        return FMath::Exp(-X*X);
    }
}

float Valley::ShoreDistance(float X, float Y)
{
    const FVector2D P(X-LakeX,Y-LakeY);
    const float Along = (.36f*P.X+.933f*P.Y)/59000.f;
    const float Across = (.933f*P.X-.36f*P.Y)/20500.f;
    const float Bend = Across + .20f*FMath::Sin(4.5f*Along+.8f) + .095f*FMath::Sin(8.f*Along);
    const float Inlets = .095f*Noise(P,12500.f,{7.2,2.4})+.035f*Noise(P,4300.f,{3.1,8.5});
    return 20500.f*(FMath::Sqrt(Along*Along+Bend*Bend)-1.f+Inlets);
}

float Valley::Height(float X, float Y)
{
    const FVector2D P(X,Y);
    const float Shore = ShoreDistance(X,Y);
    const float Land = FMath::SmoothStep(0.f,22000.f,Shore);
    float Floor = WaterLevel + FMath::Clamp(Shore*.26f,-4600.f,4400.f);
    Floor += Land*(3100.f*Noise(P,52000.f,{6.2,1.5})+1100.f*Noise(P,12500.f,{8.6,2.3}));
    // A bedrock spur interrupts the west shore and forms a small offshore island.
    Floor += 3000.f*Bell((X-LakeX+14200.f)/5400.f)*Bell((Y-LakeY-3600.f)/9200.f);
    if (P.SizeSquared() < FMath::Square(95000.)) return Floor;

    const float R = P.Size(), A = FMath::Atan2(Y,X);
    const float Warp = 26500.f*Noise(P,150000.f,{12.3,6.4}) + 9500.f*Noise(P,62000.f,{1.7,9.3});
    const float Front = Bell((R-178000.f-Warp-26500.f*FMath::Sin(3*A-.8f))/47000.f);
    const float Back = Bell((R-355000.f-1.7f*Warp-38000.f*FMath::Sin(4*A+.2f))/74000.f);
    const float Broad = Noise(P,85000.f,{4.2,8.6});
    // Warped ridged noise cuts gullies into connected massifs rather than isolated cones.
    const FVector2D Folded = P + FVector2D(Warp, -.48f*Warp);
    const float Spurs = 1.f-FMath::Abs(Noise(Folded,41000.f,{2.2,11.1}));
    const float Crags = Noise(Folded,14200.f,{7.7,5.1}) + .33f*Noise(P,5100.f,{13.2,6.3});
    const float Foothills = Front*(20500.f + 12500.f*FMath::Sin(2*A+.7f) + 22500.f*Broad + 12500.f*Spurs + 7000.f*Crags);
    const float Peaks = Back*(52500.f + 34000.f*FMath::Sin(3*A-.8f) + 49000.f*Broad + 55500.f*Spurs + 21500.f*Crags);
    return Floor + FMath::SmoothStep(95000.f,130000.f,R)*(.38f*Foothills+.32f*Peaks);
}

float Valley::Woodland(float X, float Y)
{
    const FVector2D P(X,Y);
    return .50f + .64f*Noise(P,18900.f,{2.7,8.1}) + .24f*Noise(P,5200.f,{5.1,14.6})
                + .12f*Noise(P,2200.f,{4.3,3.1});
}
