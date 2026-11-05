#include "LaunchInstallations.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

namespace
{
    FLaunchInstallation Installation(double X,ELaunchEquipment Type=ELaunchEquipment::Bungee)
    {
        FLaunchInstallation E;
        E.Type=Type; E.Anchor=FVector(X,0,1000); E.Departure=E.Anchor+FVector(7000,0,0); E.Colour=3;
        return E;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLaunchProximityTest,"SimPlaneur.Equipment.Proximity",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLaunchProximityTest::RunTest(const FString&)
{
    FLaunchInstallations World;
    const auto Original=World.Place(Installation(0));
    const auto OtherType=World.Place(Installation(0,ELaunchEquipment::Winch));
    TestEqual(TEXT("Winch and bungee limits are separate"),OtherType.NearbyRemoved,0);
    const auto Distant=World.Place(Installation(100001));
    auto Invalid=Installation(10); Invalid.Departure=Invalid.Anchor;
    TestEqual(TEXT("Invalid layout must not replace existing equipment"),World.Place(Invalid).Id,INDEX_NONE);
    TestNotNull(TEXT("Old installation survives rejected placement"),World.Find(Original.Id));
    auto High=Installation(50000); High.Anchor.Z+=5000; High.Departure.Z+=5000;
    const auto New=World.Place(High);
    TestEqual(TEXT("Exactly 500 m replaces regardless of terrain altitude"),New.NearbyRemoved,1);
    TestNull(TEXT("Replaced old anchor disappears"),World.Find(Original.Id));
    TestNotNull(TEXT("500.01 m neighbour remains"),World.Find(Distant.Id));
    TestNotNull(TEXT("Different type remains"),World.Find(OtherType.Id));
    const auto Between=World.Place(Installation(75000));
    TestEqual(TEXT("Every same-type neighbour inside radius is replaced"),Between.NearbyRemoved,2);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLaunchCapacityTest,"SimPlaneur.Equipment.Capacity",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLaunchCapacityTest::RunTest(const FString&)
{
    FLaunchInstallations World;
    TArray<int32> Bungees,Winches;
    for(int I=0;I<10;++I)
    {
        Bungees.Add(World.Place(Installation(I*60000.)).Id);
        Winches.Add(World.Place(Installation(I*60000.,ELaunchEquipment::Winch)).Id);
    }
    for(int I=0;I<10;++I) if(I!=3) World.RecordLaunch(Bungees[I]);
    const auto New=World.Place(Installation(600000));
    TestEqual(TEXT("Eleventh bungee evicts least used, not oldest"),New.LeastUsedRemoved,Bungees[3]);
    TestEqual(TEXT("Ten bungees"),World.Count(ELaunchEquipment::Bungee),10);
    TestEqual(TEXT("Ten winches unaffected"),World.Count(ELaunchEquipment::Winch),10);
    const auto NewWinch=World.Place(Installation(600000,ELaunchEquipment::Winch));
    TestEqual(TEXT("Equal use count evicts oldest unused winch"),NewWinch.LeastUsedRemoved,Winches[0]);
    // Equal counts: the less recently used one must go, independent of array order.
    World.RecordLaunch(New.Id);
    World.RecordLaunch(Bungees[0]);
    const auto Next=World.Place(Installation(660000));
    TestEqual(TEXT("Last-use tie breaker"),Next.LeastUsedRemoved,Bungees[1]);
    TestEqual(TEXT("Combined world cap"),World.Entries().Num(),20);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLaunchSaveTest,"SimPlaneur.Equipment.Persistence",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLaunchSaveTest::RunTest(const FString&)
{
    const FString Ini=FPaths::ConvertRelativePathToFull(FPaths::CreateTempFilename(*FPaths::ProjectIntermediateDir(),TEXT("LaunchInventory"),TEXT(".ini")));
    FConfigFile Empty; Empty.bCanSaveAllSections=true; GConfig->SetFile(Ini,&Empty);
    FLaunchInstallations Original,Restored;
    const int32 A=Original.Place(Installation(0)).Id;
    const int32 B=Original.Place(Installation(60000)).Id;
    Original.RecordLaunch(A); Original.RecordLaunch(A); Original.RecordLaunch(B);
    Original.Find(B)->Departure+=FVector(100,50,0);
    Original.Save(Ini,B);
    GConfig->UnloadFile(Ini); // exercise disk persistence, not the cached values
    GConfig->LoadFile(Ini); // explicit writable load; Find() alone marks arbitrary files NoSave in UE 5.8
    int32 Active=INDEX_NONE;
    TestTrue(TEXT("Load inventory"),Restored.Load(Ini,Active));
    TestEqual(TEXT("Active equipment restored"),Active,B);
    TestEqual(TEXT("Both installations restored"),Restored.Entries().Num(),2);
    if(const auto* Entry=Restored.Find(A)) TestEqual(TEXT("Launch count restored"),Entry->Launches,2);
    else AddError(TEXT("Missing saved installation"));
    if(const auto* Entry=Restored.Find(B))
    {
        TestEqual(TEXT("Colour restored"),Entry->Colour,3);
        TestTrue(TEXT("Last departure restored"),Entry->Departure.Equals(Original.Find(B)->Departure,.01));
    }
    else AddError(TEXT("Missing active installation"));
    Restored.Remove(A); Restored.Save(Ini,B);
    GConfig->UnloadFile(Ini); GConfig->LoadFile(Ini); Restored.Load(Ini,Active);
    TestNull(TEXT("Packed equipment does not reappear"),Restored.Find(A));
    TestEqual(TEXT("Remaining equipment kept"),Restored.Entries().Num(),1);
    GConfig->UnloadFile(Ini);
    IFileManager::Get().Delete(*Ini);
    return true;
}
#endif
