#pragma once
#include "CoreMinimal.h"
#include "Interfaces/IHttpRequest.h"

struct FGameRelease
{
    FString Version, Notes, Url, Sha256;
    int64 Size=0;
};

class FGameUpdater : public TSharedFromThis<FGameUpdater,ESPMode::ThreadSafe>
{
public:
    enum class EState { Idle, Checking, Current, Available, Downloading, Verifying, Ready, Error };
    static TSharedRef<FGameUpdater,ESPMode::ThreadSafe> Get();
    static bool Newer(const FString& Candidate,const FString& Installed);
    static bool SelectRelease(const FString& Json,const FString& Installed,FGameRelease& Out);
    static FString FileSha256(const FString& Path);
    static bool CanInstall();
    void Check();
    void Download();
    void Install();
    void SetAutomatic(bool Enabled);
    bool Automatic=true;
    EState State=EState::Idle;
    FString Status=TEXT("Recherche de mises à jour disponible."), InstalledVersion;
    FGameRelease Release;
    TAtomic<int64> Received{0};
private:
    void Fail(const FString& Reason);
    TSharedPtr<IHttpRequest,ESPMode::ThreadSafe> Request;
    FString DownloadPath;
};
