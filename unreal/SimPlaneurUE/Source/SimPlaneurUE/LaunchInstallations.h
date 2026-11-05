#pragma once

#include "CoreMinimal.h"

enum class ELaunchEquipment : uint8 { Bungee, Winch };

struct FLaunchInstallation
{
    int32 Id = INDEX_NONE;
    ELaunchEquipment Type = ELaunchEquipment::Bungee;
    FVector Anchor = FVector::ZeroVector, Departure = FVector::ZeroVector; // cm
    double Stiffness = 1.;
    int32 Colour = 0, Launches = 0;
    int32 LastUsed = 0;
    bool IsValid() const;
};

// Persistent world inventory, shared by high-starts and the future winch.
class FLaunchInstallations
{
public:
    static constexpr int32 MaxPerType = 10;
    static constexpr double ReplacementRadiusCm = 50000.;
    struct FPlacement { int32 Id = INDEX_NONE, NearbyRemoved = 0, LeastUsedRemoved = INDEX_NONE; };
    const TArray<FLaunchInstallation>& Entries() const { return Items; }
    FLaunchInstallation* Find(int32 Id);
    int32 Count(ELaunchEquipment Type) const;
    FPlacement Place(FLaunchInstallation Entry);
    void RecordLaunch(int32 Id);
    void Remove(int32 Id);
    void Save(const FString& Ini, int32 ActiveId) const;
    bool Load(const FString& Ini, int32& ActiveId);
private:
    TArray<FLaunchInstallation> Items;
    int32 NextId = 1, Sequence = 0;
};
