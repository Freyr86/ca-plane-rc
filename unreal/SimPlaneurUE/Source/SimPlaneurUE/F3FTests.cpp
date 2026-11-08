#include "F3F.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"
#include "HAL/FileManager.h"
namespace
{
    FF3FCourse Course()
    { FF3FCourse C; C.A={0,0,0}; C.B={10000,0,200}; C.FlyingSide={0,1,0}; C.Installed=true; return C; }
    void Move(FF3FRun& R,double X,double Y,double Dt=1.,double Z=1000.)
    { R.Step(Course(),{{X,100,Z}},{{Y,100,Z}},Dt); }
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FF3FTimingTest,"SimPlaneur.F3F.TenLegs",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FF3FTimingTest::RunTest(const FString&)
{
    FF3FRun R; R.Arm(); R.Launch(0);
    Move(R,-100,100); TestEqual(TEXT("Entry bip"),R.Signals,1);
    TestEqual(TEXT("Interpolated start"),R.StartTime,.5);
    Move(R,100,9900); Move(R,9900,5000); TestEqual(TEXT("Short turn rejected"),R.Legs,0);
    Move(R,5000,9900);
    for(int I=0;I<10;++I) Move(R,I%2?100:-100,I%2?-100:10100);
    TestEqual(TEXT("Ten legs"),R.Legs,10);
    TestTrue(TEXT("Finished"),R.State==FF3FRun::EState::Finished);
    TestEqual(TEXT("Entry plus ten signals"),R.Signals,11);
    TestEqual(TEXT("Ten split times"),R.Splits.Num(),10);
    const double Final=R.Time(); Move(R,-100,10100); TestEqual(TEXT("Final frozen"),R.Time(),Final);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FF3FEntryTest,"SimPlaneur.F3F.EntryAndLateStart",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FF3FEntryTest::RunTest(const FString&)
{
    FF3FRun R; R.Arm(); R.Now=12.; R.Launch(0);
    Move(R,100,-100); TestEqual(TEXT("Wrong direction is not entry"),R.Signals,0);
    Move(R,-100,-200,30.);
    TestTrue(TEXT("Late clock"),R.Late); TestEqual(TEXT("Clock starts at launch+30"),R.StartTime,42.);
    Move(R,-100,100,4.); TestEqual(TEXT("Late entry retains clock"),R.StartTime,42.);
    TestEqual(TEXT("Late entry does not count a leg"),R.Legs,0);
    TestEqual(TEXT("Time includes late entry"),R.Time(),5.);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FF3FWingTest,"SimPlaneur.F3F.WingAndInfinitePlane",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FF3FWingTest::RunTest(const FString&)
{
    FF3FRun R; R.Arm(); R.Launch(0); Move(R,-100,100);
    R.Step(Course(),{{9700,100,50000},{9900,100,50000}},{{9900,100,50000},{10100,100,50000}},1.);
    TestEqual(TEXT("Wing counts before fuselage even above visual grid"),R.Legs,1);
    R.Step(Course(),{{9900,100,50000},{10100,100,50000}},{{10000,100,50000},{10200,100,50000}},1.);
    TestEqual(TEXT("No repeated signal while outside"),R.Legs,1);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FF3FClearanceTest,"SimPlaneur.F3F.ClearanceAndSafety",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FF3FClearanceTest::RunTest(const FString&)
{
    FF3FRun R; R.Arm(); R.Launch(2000); Move(R,-100,100);
    R.Step(Course(),{{100,100,1000}},{{100,-100,1000}},1.);
    TestEqual(TEXT("Safety crossing"),R.SafetyCrossings,1);
    R.Step(Course(),{{100,-100,1000}},{{200,-100,1000}},1.);
    TestEqual(TEXT("No repeat inside safety area"),R.SafetyCrossings,1);
    for(int I=0;I<10;++I) Move(R,I%2?100:-100,I%2?-100:10100);
    TestTrue(TEXT("Height check pending"),R.State==FF3FRun::EState::Clearance);
    Move(R,-100,-100,6.); TestTrue(TEXT("Failed 5s clearance"),R.State==FF3FRun::EState::Invalid);
    FF3FRun Good; Good.Arm(); Good.Launch(2000); Move(Good,-100,100);
    for(int I=0;I<10;++I) Move(Good,I%2?100:-100,I%2?-100:10100);
    Move(Good,-100,-100,1.,2100.); TestTrue(TEXT("Successful clearance"),Good.State==FF3FRun::EState::Finished);
    FF3FRun Late; Late.Arm(); Late.Launch(2000); Move(Late,-100,100);
    for(int I=0;I<10;++I) Move(Late,I%2?100:-100,I%2?-100:10100);
    Late.Step(Course(),{{-100,100,1000}},{{-100,100,2001}},6.);
    TestTrue(TEXT("Height crossed after deadline is rejected"),Late.State==FF3FRun::EState::Invalid);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FF3FWeatherTest,"SimPlaneur.F3F.WeatherAndAbort",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FF3FWeatherTest::RunTest(const FString&)
{
    FF3FRun R; R.Arm(); R.Launch(0);
    R.Weather(false,false,19.); TestFalse(TEXT("19 seconds allowed"),R.WeatherRetry);
    R.Weather(true,false,1.); R.Weather(false,false,19.); TestFalse(TEXT("Good wind resets duration"),R.WeatherRetry);
    R.Weather(false,false,1.); TestTrue(TEXT("20 seconds offers retry"),R.WeatherRetry);
    R.Invalidate(TEXT("Crash")); Move(R,-100,10100); TestEqual(TEXT("No scoring after crash"),R.Legs,0);
    R.Arm(); R.Launch(0); R.Weather(true,true,.1); TestTrue(TEXT("Rain immediate"),R.WeatherRetry);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FF3FPersistenceTest,"SimPlaneur.F3F.GeometryAndPersistence",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FF3FPersistenceTest::RunTest(const FString&)
{
    auto C=Course(); TestTrue(TEXT("Different ground heights accepted"),C.Valid());
    TestEqual(TEXT("Gate normal orthogonal to side"),FVector::DotProduct(C.Axis(),C.FlyingSide),0.);
    const FString Ini=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("Tests/F3F.ini"));
    FConfigFile Empty; Empty.bCanSaveAllSections=true; GConfig->SetFile(Ini,&Empty);
    C.Gates=false; C.Save(Ini); GConfig->UnloadFile(Ini); GConfig->LoadFile(Ini);
    FF3FCourse Loaded; Loaded.Load(Ini);
    TestTrue(TEXT("Saved course"),Loaded.Valid()); TestFalse(TEXT("Gates choice saved"),Loaded.Gates);
    TestEqual(TEXT("Base B saved"),Loaded.B,C.B);
    C.B.X=9990; TestFalse(TEXT("Reject non100m"),C.Valid());
    GConfig->UnloadFile(Ini); IFileManager::Get().Delete(*Ini); return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FF3FVisibilityTest,"SimPlaneur.F3F.JudgeVisibility",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FF3FVisibilityTest::RunTest(const FString&)
{
    FF3FRun R; R.Arm(); R.Launch(0);
    R.Step(Course(),{{-100,100,1000}},{{100,100,1000}},1.,false);
    TestTrue(TEXT("Unseen entry invalid"),R.State==FF3FRun::EState::Invalid);
    R.Arm(); R.Launch(0); Move(R,-100,100);
    R.Step(Course(),{{9900,100,1000}},{{10100,100,1000}},1.,false);
    TestEqual(TEXT("Hidden turn has no bip"),R.Legs,0);
    R.Step(Course(),{{10100,100,1000}},{{10200,100,1000}},1.,true);
    TestEqual(TEXT("Signal when visible outside"),R.Legs,1);
    TestEqual(TEXT("Delayed turn timestamp"),R.Splits[0],2.5);
    return true;
}
#endif
