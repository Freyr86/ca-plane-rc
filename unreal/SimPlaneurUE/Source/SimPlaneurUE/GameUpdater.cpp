#include "GameUpdater.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Async/Async.h"
#include "Engine/Engine.h"
#include "GameFramework/GameUserSettings.h"
#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#include <bcrypt.h>
#endif

namespace
{
    const FString DownloadRoot=TEXT("https://github.com/Freyr86/ca-plane-rc/releases/download/");
    bool VersionParts(FString Text,FIntVector& V)
    {
        Text.RemoveFromStart(TEXT("v"));
        TArray<FString> Parts; Text.ParseIntoArray(Parts,TEXT("."),false);
        if(Parts.Num()!=3) return false;
        for(int32 I=0;I<3;++I)
        {
            if(Parts[I].IsEmpty() || Parts[I].Len()>6) return false;
            for(TCHAR C:Parts[I]) if(C<'0' || C>'9') return false;
            V[I]=FCString::Atoi(*Parts[I]);
        }
        return true;
    }
    bool ValidHash(const FString& Hash)
    {
        if(Hash.Len()!=64) return false;
        for(TCHAR C:Hash) if(!FChar::IsHexDigit(C)) return false;
        return true;
    }
    TSharedRef<IHttpRequest,ESPMode::ThreadSafe> NewRequest(const FString& Url,float Timeout)
    {
        auto R=FHttpModule::Get().CreateRequest(); R->SetURL(Url); R->SetVerb(TEXT("GET"));
        R->SetHeader(TEXT("User-Agent"),TEXT("CaPlaneRC-Updater")); R->SetTimeout(Timeout);
        R->SetDelegateThreadPolicy(EHttpRequestDelegateThreadPolicy::CompleteOnGameThread);
        return R;
    }
}

bool FGameUpdater::Newer(const FString& Candidate,const FString& Installed)
{
    FIntVector A,B;
    if(!VersionParts(Candidate,A) || !VersionParts(Installed,B)) return false;
    return A.X!=B.X?A.X>B.X:A.Y!=B.Y?A.Y>B.Y:A.Z>B.Z;
}

bool FGameUpdater::SelectRelease(const FString& Json,const FString& Installed,FGameRelease& Out)
{
    TArray<TSharedPtr<FJsonValue>> Releases;
    if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),Releases)) return false;
    Out=FGameRelease();
    for(const auto& Item:Releases)
    {
        const TSharedPtr<FJsonObject>* Obj=nullptr;
        if(!Item->TryGetObject(Obj)) continue;
        bool Draft=true; FString Tag,Notes;
        if(!(*Obj)->TryGetBoolField(TEXT("draft"),Draft) || Draft || !(*Obj)->TryGetStringField(TEXT("tag_name"),Tag)) continue;
        if(!Tag.StartsWith(TEXT("v")) || !Newer(Tag,Out.Version.IsEmpty()?Installed:Out.Version)) continue;
        (*Obj)->TryGetStringField(TEXT("body"),Notes);
        const TArray<TSharedPtr<FJsonValue>>* Assets=nullptr;
        if(!(*Obj)->TryGetArrayField(TEXT("assets"),Assets)) continue;
        const FString Name=TEXT("CaPlaneRC-")+Tag.Mid(1)+TEXT("-Windows-Setup.exe");
        for(const auto& Asset:*Assets)
        {
            const TSharedPtr<FJsonObject>* A=nullptr; FString N,Url,Digest,UploadState; double Size=0;
            if(!Asset->TryGetObject(A) || !(*A)->TryGetStringField(TEXT("name"),N) || N!=Name
                || !(*A)->TryGetStringField(TEXT("browser_download_url"),Url) || Url!=DownloadRoot+Tag+TEXT("/")+Name
                || !(*A)->TryGetStringField(TEXT("state"),UploadState) || UploadState!=TEXT("uploaded")
                || !(*A)->TryGetNumberField(TEXT("size"),Size) || Size<=0 || Size>2147483647.
                || !(*A)->TryGetStringField(TEXT("digest"),Digest) || !Digest.RemoveFromStart(TEXT("sha256:")) || !ValidHash(Digest)) continue;
            Out={Tag.Mid(1),Notes.Left(16000),Url,Digest.ToLower(),int64(Size)};
        }
    }
    return true;
}

TSharedRef<FGameUpdater,ESPMode::ThreadSafe> FGameUpdater::Get()
{
    static TSharedRef<FGameUpdater,ESPMode::ThreadSafe> Instance=[] {
        auto U=MakeShared<FGameUpdater,ESPMode::ThreadSafe>();
        GConfig->GetString(TEXT("/Script/EngineSettings.GeneralProjectSettings"),TEXT("ProjectVersion"),U->InstalledVersion,GGameIni);
        GConfig->GetBool(TEXT("SimPlaneur.Updates"),TEXT("Automatic"),U->Automatic,GGameUserSettingsIni);
        return U;
    }();
    return Instance;
}

void FGameUpdater::SetAutomatic(bool Enabled)
{
    Automatic=Enabled;
    GConfig->SetBool(TEXT("SimPlaneur.Updates"),TEXT("Automatic"),Enabled,GGameUserSettingsIni);
    GConfig->Flush(false,GGameUserSettingsIni);
}

void FGameUpdater::Fail(const FString& Reason) { Request.Reset(); State=EState::Error; Status=Reason; }

void FGameUpdater::Check()
{
    if(State==EState::Checking || State==EState::Downloading || State==EState::Verifying || State==EState::Ready) return;
    State=EState::Checking; Status=TEXT("Recherche sur GitHub…");
    Request=NewRequest(TEXT("https://api.github.com/repos/Freyr86/ca-plane-rc/releases?per_page=100"),20);
    Request->SetHeader(TEXT("Accept"),TEXT("application/vnd.github+json"));
    Request->OnProcessRequestComplete().BindLambda([Self=AsShared()](FHttpRequestPtr,FHttpResponsePtr Response,bool OK) {
        Self->Request.Reset();
        if(!OK || !Response.IsValid() || Response->GetResponseCode()!=200)
        {Self->Fail(TEXT("Vérification indisponible. Tu peux continuer à jouer et réessayer plus tard."));return;}
        if(!SelectRelease(Response->GetContentAsString(),Self->InstalledVersion,Self->Release))
        {Self->Fail(TEXT("Réponse de mise à jour invalide. Réessaie plus tard."));return;}
        const bool Found=!Self->Release.Version.IsEmpty();
        Self->State=Found?EState::Available:EState::Current;
        Self->Status=Found?TEXT("Version ")+Self->Release.Version+TEXT(" disponible."):TEXT("Tu disposes de la dernière version compatible.");
    });
    if(!Request->ProcessRequest()) Fail(TEXT("Connexion impossible. Le jeu reste disponible hors ligne."));
}

FString FGameUpdater::FileSha256(const FString& Path)
{
#if PLATFORM_WINDOWS
    TUniquePtr<FArchive> File(IFileManager::Get().CreateFileReader(*Path));
    if(!File) return {};
    BCRYPT_ALG_HANDLE Algorithm=nullptr; BCRYPT_HASH_HANDLE Hash=nullptr;
    if(BCryptOpenAlgorithmProvider(&Algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0) return {};
    if(BCryptCreateHash(Algorithm,&Hash,nullptr,0,nullptr,0,0)<0) {BCryptCloseAlgorithmProvider(Algorithm,0);return {};}
    TArray<uint8> Buffer; Buffer.SetNumUninitialized(1024*1024); bool OK=true;
    while(File->Tell()<File->TotalSize() && OK)
    {
        const int64 N=FMath::Min<int64>(Buffer.Num(),File->TotalSize()-File->Tell());
        File->Serialize(Buffer.GetData(),N);
        OK=!File->IsError() && BCryptHashData(Hash,Buffer.GetData(),ULONG(N),0)>=0;
    }
    uint8 Digest[32]; OK=OK && BCryptFinishHash(Hash,Digest,32,0)>=0;
    BCryptDestroyHash(Hash); BCryptCloseAlgorithmProvider(Algorithm,0);
    return OK?BytesToHex(Digest,32).ToLower():FString();
#else
    return {};
#endif
}

void FGameUpdater::Download()
{
    if(State!=EState::Available) return;
    const FString Dir=FPaths::Combine(FPlatformProcess::UserSettingsDir(),TEXT("CaPlaneRC/Updates"));
    IFileManager::Get().MakeDirectory(*Dir,true);
    DownloadPath=Dir/TEXT("CaPlaneRC-")+Release.Version+TEXT("-Windows-Setup.exe");
    auto* Writer=IFileManager::Get().CreateFileWriter(*DownloadPath);
    if(!Writer) {Fail(TEXT("Impossible de créer le téléchargement. Vérifie l’espace disque et réessaie."));return;}
    TSharedRef<FArchive> Stream=MakeShareable(Writer);
    State=EState::Downloading; Received=0; Status=TEXT("Téléchargement en cours… Tu peux continuer à jouer.");
    Request=NewRequest(Release.Url,1800);
    if(!Request->SetResponseBodyReceiveStream(Stream)) {Fail(TEXT("Téléchargement indisponible."));return;}
    Request->OnRequestProgress64().BindLambda([Self=AsShared()](FHttpRequestPtr,int64,int64 Bytes){Self->Received=Bytes;});
    Request->OnProcessRequestComplete().BindLambda([Self=AsShared(),Stream](FHttpRequestPtr,FHttpResponsePtr Response,bool OK) {
        const bool Written=Stream->Close() && !Stream->IsError(); Self->Request.Reset();
        if(!OK || !Response.IsValid() || Response->GetResponseCode()!=200 || !Written
            || IFileManager::Get().FileSize(*Self->DownloadPath)!=Self->Release.Size)
        {Self->Fail(TEXT("Téléchargement incomplet. Vérifie la connexion et l’espace disque, puis réessaie."));return;}
        Self->State=EState::Verifying; Self->Status=TEXT("Vérification du fichier téléchargé…");
        Async(EAsyncExecution::ThreadPool,[Self] {
            const FString Digest=FileSha256(Self->DownloadPath);
            AsyncTask(ENamedThreads::GameThread,[Self,Digest] {
                if(Digest!=Self->Release.Sha256) {Self->Fail(TEXT("Le fichier n’a pas passé la vérification. Il ne sera pas exécuté."));return;}
                Self->State=EState::Ready; Self->Status=TEXT("Mise à jour prête. Récupère le planeur avant de l’installer.");
            });
        });
    });
    if(!Request->ProcessRequest()) Fail(TEXT("Impossible de démarrer le téléchargement."));
}

bool FGameUpdater::CanInstall()
{
    return PLATFORM_WINDOWS && !WITH_EDITOR;
}

void FGameUpdater::Install()
{
    if(State!=EState::Ready || !CanInstall()) return;
    State=EState::Verifying;
    Status=TEXT("Préparation de l’installation…");
    if(IFileManager::Get().FileSize(*DownloadPath)!=Release.Size || FileSha256(DownloadPath)!=Release.Sha256)
    {Fail(TEXT("Le fichier a changé depuis sa vérification. Télécharge-le à nouveau."));return;}
    // Same per-user directory and AppId: upgrade in place; user settings excluded.
    const FString Root=FPaths::ConvertRelativePathToFull(FPaths::Combine(FPlatformProcess::BaseDir(),TEXT("../../..")));
    const FString Args=FString::Printf(TEXT("/SP- /SILENT /NORESTART /CLOSEAPPLICATIONS /NOFORCECLOSEAPPLICATIONS /RESTARTGAME=1 /DIR=\"%s\""),*Root);
    if(GEngine && GEngine->GetGameUserSettings()) GEngine->GetGameUserSettings()->SaveSettings();
    GConfig->Flush(false);
    auto Process=FPlatformProcess::CreateProc(*DownloadPath,*Args,true,false,false,nullptr,0,nullptr,nullptr);
    if(!Process.IsValid()) {Fail(TEXT("L’installeur n’a pas démarré. Le simulateur reste ouvert."));return;}
    FPlatformProcess::CloseProc(Process);
    FPlatformMisc::RequestExit(false);
}
