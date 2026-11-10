#include "PostStallPolar.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPostStallPolarTest,"SimPlaneur.Physics.PostStallPolar",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPostStallPolarTest::RunTest(const FString&)
{
    const TArray<double> Angles={FMath::DegreesToRadians(-40.),FMath::DegreesToRadians(-10.),0.,
        FMath::DegreesToRadians(10.),FMath::DegreesToRadians(40.)};
    const TArray<double> Lift={-.36,-.8,.1,.9,.43},Speeds={10.,20.};
    const TArray<double> Drag={.65,.64,.04,.03,.02,.01,.04,.03,.65,.64};
    auto Sample=[&](double Alpha,double Speed=15.) {
        FVector2D Result;
        PostStallPolar::Sample(Angles,Lift,Drag,Speeds,Alpha,Speed,Result.X,Result.Y);
        return Result;
    };
    const FVector2D Within=Sample(FMath::DegreesToRadians(5.));
    TestTrue(TEXT("Measured lift interpolation is preserved"),FMath::IsNearlyEqual(Within.X,.5,1.e-12));
    TestTrue(TEXT("Measured drag interpolation is preserved"),FMath::IsNearlyEqual(Within.Y,.025,1.e-12));
    for (int32 I=0;I<Angles.Num();++I)
    {
        const auto Value=Sample(Angles[I],10.);
        TestEqual(TEXT("Measured lift samples are unchanged"),Value.X,Lift[I]);
        TestEqual(TEXT("Measured drag samples are unchanged"),Value.Y,Drag[2*I]);
    }
    const double Epsilon=1.e-6;
    for (const double Degrees : {-180.,-90.,-60.,-40.,40.,60.,90.,180.})
    {
        const double A=FMath::DegreesToRadians(Degrees);
        const FVector2D Before=Sample(A-Epsilon), At=Sample(A), After=Sample(A+Epsilon);
        TestTrue(TEXT("Full-angle polar has no value discontinuities"),Before.Equals(After,1.e-4));
        const FVector2D LeftSlope=(At-Before)/Epsilon, RightSlope=(After-At)/Epsilon;
        TestTrue(FString::Printf(TEXT("Polar slopes at %.0f deg: left=(%.8f,%.8f), right=(%.8f,%.8f)"),
            Degrees,LeftSlope.X,LeftSlope.Y,RightSlope.X,RightSlope.Y),LeftSlope.Equals(RightSlope,1.e-3));
        if (FMath::Abs(Degrees)==90. || FMath::Abs(Degrees)==180.)
            TestTrue(TEXT("Lift vanishes broadside and in reversed flow"),FMath::Abs(At.X)<1.e-12);
    }
    TestTrue(TEXT("Broadside drag replaces the old endpoint clamp"),Sample(.5*UE_DOUBLE_PI).Y>Sample(Angles.Last()).Y);
    TestTrue(TEXT("The explicit Cd90 parameter is respected"),[&] {
        double CL,CD; PostStallPolar::Sample(Angles,Lift,Drag,Speeds,.5*UE_DOUBLE_PI,15.,CL,CD,1.6);
        return FMath::Abs(CL)<1.e-12 && FMath::IsNearlyEqual(CD,1.6,1.e-12);
    }());
    for (int32 Degrees=-540;Degrees<=540;++Degrees)
    {
        const double A=FMath::DegreesToRadians(double(Degrees));
        const auto Coeff=Sample(A);
        TestTrue(TEXT("Finite and nonnegative drag over the full circle"),
            FMath::IsFinite(Coeff.X) && FMath::IsFinite(Coeff.Y) && Coeff.Y>=0.);
        TestTrue(TEXT("Polar repeats every full revolution"),Coeff.Equals(Sample(A+2.*UE_DOUBLE_PI),1.e-10));
        const FVector Air(15.*FMath::Cos(A),0,15.*FMath::Sin(A));
        const FVector LiftDirection(-FMath::Sin(A),0,FMath::Cos(A));
        const FVector Force=-Coeff.Y*Air.GetSafeNormal()-Coeff.X*LiftDirection;
        TestTrue(TEXT("Aerodynamic force cannot add energy to the airflow"),FVector::DotProduct(Force,Air)<=1.e-10);
        const auto ZeroSpeed=Sample(A,0.);
        TestTrue(TEXT("Coefficients remain finite at zero airspeed"),FMath::IsFinite(ZeroSpeed.X) && FMath::IsFinite(ZeroSpeed.Y));
    }
    return !HasAnyErrors();
}
#endif
