#include "LaunchInstallations.h"
#include "WinchLaunch.h"
#include "Misc/ConfigCacheIni.h"

namespace { const TCHAR* Root = TEXT("SimPlaneur.LaunchEquipment"); }

bool FLaunchInstallation::IsValid() const
{
    const double Length = FVector::Distance(Anchor,Departure);
    return (Type==ELaunchEquipment::Bungee || Type==ELaunchEquipment::Winch)
        && !Anchor.ContainsNaN() && !Departure.ContainsNaN() && FMath::IsFinite(Length)
        && FMath::IsFinite(Stiffness) && Stiffness>0. && Length>=(Type==ELaunchEquipment::Bungee?500.:FWinchDrive::MinLayout*100.)
        && Length<=(Type==ELaunchEquipment::Bungee ? 9200. : FWinchDrive::MaxLayout*100.);
}

FLaunchInstallation* FLaunchInstallations::Find(int32 Id)
{ return Items.FindByPredicate([&](const auto& E){return E.Id==Id;}); }

int32 FLaunchInstallations::Count(ELaunchEquipment Type) const
{
    int32 N=0;
    for(const auto& E:Items) if(E.Type==Type) ++N;
    return N;
}

FLaunchInstallations::FPlacement FLaunchInstallations::Place(FLaunchInstallation Entry)
{
    FPlacement Result;
    if(!Entry.IsValid()) return Result;
    // Replacement is committed only once BOTH endpoints are valid. Cancelling
    // a draft never removes the equipment already installed in the world.
    Result.NearbyRemoved=Items.RemoveAll([&](const auto& E){return E.Type==Entry.Type
        && FVector::DistSquared2D(E.Anchor,Entry.Anchor)<=FMath::Square(ReplacementRadiusCm);});
    if(Count(Entry.Type)>=MaxPerType)
    {
        const FLaunchInstallation* Least=nullptr;
        for(const auto& E:Items)
            if(E.Type==Entry.Type && (!Least || E.Launches<Least->Launches
                || (E.Launches==Least->Launches && E.LastUsed<Least->LastUsed))) Least=&E;
        Result.LeastUsedRemoved=Least->Id;
        Remove(Least->Id);
    }
    Entry.Id=NextId++;
    Entry.Launches=0;
    Entry.LastUsed=++Sequence;
    Result.Id=Entry.Id;
    Items.Add(Entry);
    return Result;
}

void FLaunchInstallations::RecordLaunch(int32 Id)
{
    if(auto* E=Find(Id)) { E->Launches=FMath::Min(E->Launches,MAX_int32-1)+1; E->LastUsed=++Sequence; }
}
void FLaunchInstallations::Remove(int32 Id)
{ Items.RemoveAll([&](const auto& E){return E.Id==Id;}); }

void FLaunchInstallations::Save(const FString& Ini,int32 ActiveId) const
{
    GConfig->SetInt(Root,TEXT("Version"),1,Ini);
    GConfig->SetInt(Root,TEXT("Count"),Items.Num(),Ini);
    GConfig->SetInt(Root,TEXT("ActiveId"),ActiveId,Ini);
    GConfig->SetInt(Root,TEXT("NextId"),NextId,Ini);
    GConfig->SetInt(Root,TEXT("Sequence"),Sequence,Ini);
    for(int32 I=0;I<MaxPerType*2;++I)
    {
        const FString Sec=FString::Printf(TEXT("%s.%d"),Root,I);
        // Fixed slots are invalidated in the same transaction. EmptySection
        // flushes immediately in UE and would write an incomplete inventory.
        if(!Items.IsValidIndex(I)) { GConfig->SetInt(*Sec,TEXT("Id"),INDEX_NONE,Ini); continue; }
        const auto& E=Items[I];
        GConfig->SetInt(*Sec,TEXT("Id"),E.Id,Ini);
        GConfig->SetInt(*Sec,TEXT("Type"),int32(E.Type),Ini);
        GConfig->SetVector(*Sec,TEXT("Anchor"),E.Anchor,Ini);
        GConfig->SetVector(*Sec,TEXT("Departure"),E.Departure,Ini);
        GConfig->SetDouble(*Sec,TEXT("Stiffness"),E.Stiffness,Ini);
        GConfig->SetInt(*Sec,TEXT("Colour"),E.Colour,Ini);
        GConfig->SetInt(*Sec,TEXT("Launches"),E.Launches,Ini);
        GConfig->SetInt(*Sec,TEXT("LastUsed"),E.LastUsed,Ini);
    }
    GConfig->Flush(false,Ini);
}

bool FLaunchInstallations::Load(const FString& Ini,int32& ActiveId)
{
    Items.Reset(); NextId=1; Sequence=0; ActiveId=INDEX_NONE;
    int32 Version=0,N=0;
    if(!GConfig->GetInt(Root,TEXT("Version"),Version,Ini) || Version!=1) return false;
    GConfig->GetInt(Root,TEXT("Count"),N,Ini);
    GConfig->GetInt(Root,TEXT("ActiveId"),ActiveId,Ini);
    GConfig->GetInt(Root,TEXT("NextId"),NextId,Ini);
    GConfig->GetInt(Root,TEXT("Sequence"),Sequence,Ini);
    NextId=FMath::Clamp(NextId,1,MAX_int32-100);
    Sequence=FMath::Clamp(Sequence,0,MAX_int32-100);
    for(int32 I=0;I<FMath::Clamp(N,0,MaxPerType*2);++I)
    {
        const FString Sec=FString::Printf(TEXT("%s.%d"),Root,I);
        FLaunchInstallation E;
        int32 Type=-1;
        if(!GConfig->GetInt(*Sec,TEXT("Id"),E.Id,Ini) || E.Id<=0 || E.Id>=MAX_int32-100 || Find(E.Id)
            || !GConfig->GetInt(*Sec,TEXT("Type"),Type,Ini) || Type<0 || Type>1
            || !GConfig->GetVector(*Sec,TEXT("Anchor"),E.Anchor,Ini)
            || !GConfig->GetVector(*Sec,TEXT("Departure"),E.Departure,Ini)
            || !GConfig->GetDouble(*Sec,TEXT("Stiffness"),E.Stiffness,Ini)) continue;
        E.Type=ELaunchEquipment(Type);
        if(!E.IsValid() || Count(E.Type)>=MaxPerType) continue;
        GConfig->GetInt(*Sec,TEXT("Colour"),E.Colour,Ini);
        GConfig->GetInt(*Sec,TEXT("Launches"),E.Launches,Ini);
        GConfig->GetInt(*Sec,TEXT("LastUsed"),E.LastUsed,Ini);
        E.Colour=FMath::Clamp(E.Colour,0,5);
        E.Launches=FMath::Max(0,E.Launches);
        E.LastUsed=FMath::Clamp(E.LastUsed,0,MAX_int32-100);
        NextId=FMath::Max(NextId,E.Id+1); Sequence=FMath::Max(Sequence,E.LastUsed);
        Items.Add(E);
    }
    if(!Find(ActiveId)) ActiveId=INDEX_NONE;
    return true;
}
