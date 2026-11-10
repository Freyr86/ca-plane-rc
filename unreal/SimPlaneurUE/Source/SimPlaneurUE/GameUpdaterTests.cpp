#include "GameUpdater.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/FileManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUpdateSelectionTest,"SimPlaneur.Updates.ReleaseSelection",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FUpdateSelectionTest::RunTest(const FString&)
{
    TestTrue(TEXT("Numeric version ordering"),FGameUpdater::Newer(TEXT("v0.5.10"),TEXT("0.5.9")));
    TestFalse(TEXT("No downgrade"),FGameUpdater::Newer(TEXT("0.5.6"),TEXT("0.5.7")));
    TestFalse(TEXT("No same-version installation"),FGameUpdater::Newer(TEXT("v0.5.7"),TEXT("0.5.7")));
    TestFalse(TEXT("Reject version path traversal"),FGameUpdater::Newer(TEXT("v1.2.3/../evil"),TEXT("0.5.7")));
    const FString Json=TEXT(R"([{"tag_name":"v0.5.8","draft":false,"prerelease":true,"body":"Tutoriel","assets":[{"name":"CaPlaneRC-0.5.8-Windows-Setup.exe","state":"uploaded","size":1234,"digest":"sha256:ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad","browser_download_url":"https://github.com/Freyr86/ca-plane-rc/releases/download/v0.5.8/CaPlaneRC-0.5.8-Windows-Setup.exe"}]}])");
    FGameRelease R;
    TestTrue(TEXT("Read beta metadata"),FGameUpdater::SelectRelease(Json,TEXT("0.5.7"),R));
    TestEqual(TEXT("Select published beta installer"),R.Version,FString(TEXT("0.5.8")));
    FGameUpdater::SelectRelease(Json.Replace(TEXT("\"draft\":false"),TEXT("\"draft\":true")),TEXT("0.5.7"),R);
    TestTrue(TEXT("Never expose drafts"),R.Version.IsEmpty());
    FGameUpdater::SelectRelease(Json.Replace(TEXT("https://github.com/"),TEXT("https://evil.example/")),TEXT("0.5.7"),R);
    TestTrue(TEXT("Reject off-repository executable"),R.Version.IsEmpty());
    FGameUpdater::SelectRelease(Json.Replace(TEXT("sha256:"),TEXT("sha1:")),TEXT("0.5.7"),R);
    TestTrue(TEXT("Require checksum"),R.Version.IsEmpty());
    TestFalse(TEXT("Malformed server data is an error"),FGameUpdater::SelectRelease(TEXT("{}"),TEXT("0.5.7"),R));
    const FString File=FPaths::CreateTempFilename(*FPaths::ProjectSavedDir(),TEXT("update-hash-"),TEXT(".tmp"));
    FFileHelper::SaveStringToFile(TEXT("abc"),*File,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    TestEqual(TEXT("Streamed SHA256 known vector"),FGameUpdater::FileSha256(File),FString(TEXT("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")));
    FFileHelper::SaveStringToFile(TEXT("abd"),*File,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    TestNotEqual(TEXT("Tampering changes digest"),FGameUpdater::FileSha256(File),FString(TEXT("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")));
    IFileManager::Get().Delete(*File);
    return true;
}

DEFINE_LATENT_AUTOMATION_COMMAND_THREE_PARAMETER(FPublishedUpdateWait,FAutomationTestBase*,Test,double,Started,FString,OriginalVersion);
bool FPublishedUpdateWait::Update()
{
    auto U=FGameUpdater::Get(); using S=FGameUpdater::EState;
    if(U->State==S::Available) {U->Download();return false;}
    if(U->State==S::Ready)
    {
        Test->TestTrue(TEXT("Published installer downloaded and SHA256 verified"),U->Received.Load()>0);
        U->Install(); Test->TestEqual(TEXT("Editor cannot execute an installer"),int32(U->State),int32(S::Ready));
        U->InstalledVersion=OriginalVersion; return true;
    }
    if(U->State==S::Error || FPlatformTime::Seconds()-Started>300)
    {Test->AddError(U->Status);U->InstalledVersion=OriginalVersion;return true;}
    return false;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPublishedUpdateTest,"SimPlaneur.Updates.PublishedDownload",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPublishedUpdateTest::RunTest(const FString&)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("UpdateDownloadProof")))
    {AddInfo(TEXT("Network/download proof is opt-in: -UpdateDownloadProof"));return true;}
    auto U=FGameUpdater::Get(); const FString Original=U->InstalledVersion;
    U->InstalledVersion=TEXT("0.0.0"); U->Check();
    ADD_LATENT_AUTOMATION_COMMAND(FPublishedUpdateWait(this,FPlatformTime::Seconds(),Original));
    return true;
}
#endif
