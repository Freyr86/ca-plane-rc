#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "GameProcess.h"
#include "RuntimePrerequisites.h"
#include <iostream>
#include <vector>

namespace fs=std::filesystem;

int wmain(int argc,wchar_t** argv)
{
    wchar_t module[32768]{};GetModuleFileNameW(nullptr,module,DWORD(std::size(module)));
    const fs::path executable=module;
    if(argc>1 && std::wstring(argv[1])==L"--probe-child")
    {
        // A bootstrap succeeds regardless of the real game. The runtime also
        // verifies arguments and its installed working directory before exit.
        if(executable.parent_path().filename()!=L"Win64") return 0;
        if(argc!=7 || std::wstring(argv[3])!=L"two words" || std::wstring(argv[4])!=L"quoted \"value\""
            || std::wstring(argv[5])!=L"C:\\path with spaces\\" || std::wstring(argv[6])!=L""
            || fs::current_path()!=executable.parent_path().parent_path().parent_path().parent_path()) return 91;
        ExitProcess(DWORD(std::stoul(argv[2])));
    }

    // Isolate each run below the build output, with a space in the install path.
    const auto root=fs::current_path()/(L"Game process fixture "+std::to_wstring(GetCurrentProcessId()));
    const auto runtime=CaPlane::GameExecutable(root);
    fs::create_directories(runtime.parent_path());
    fs::copy_file(executable,root/L"SimPlaneurUE.exe");
    fs::copy_file(executable,runtime);
    int failures=0;
    auto Check=[&](bool success,const char* name)
    {std::cout<<(success?"PASS ":"FAIL ")<<name<<'\n';if(!success)++failures;};
    using CaPlane::RuntimeAction;
    Check(!CaPlane::RuntimeVersionSupported({}),"missing VC++ runtime requires attention");
    Check(!CaPlane::RuntimeVersionSupported({true,14,0,0}),"old VC++ runtime requires attention");
    Check(CaPlane::RuntimeVersionSupported({true,14,_MSC_VER%100,_MSC_FULL_VER%100000}),"matching toolchain runtime is accepted");
    Check(CaPlane::RuntimeVersionSupported({true,14,99,0}),"newer runtime is accepted without downgrade");
    Check(CaPlane::RuntimeNextAction(true,false,false)==RuntimeAction::Continue,"ready runtime never offers installer");
    Check(CaPlane::RuntimeNextAction(false,true,true)==RuntimeAction::StopUnattended,"unattended never starts prerequisite installer");
    Check(CaPlane::RuntimeNextAction(false,false,true)==RuntimeAction::OfferInstaller,"missing runtime offers explicit installation choice");
    Check(CaPlane::RuntimeNextAction(false,false,false)==RuntimeAction::ReportMissing,"missing bundled installer offers report instead");
    for(DWORD expected:{DWORD(0),DWORD(3),DWORD(0xc0000005)})
    {
        std::wstring command=CaPlane::QuoteArgument(runtime.wstring());
        const std::vector<std::wstring> args={L"--probe-child",std::to_wstring(expected),L"two words",L"quoted \"value\"",L"C:\\path with spaces\\",L""};
        for(const auto& argument:args) command+=L" "+CaPlane::QuoteArgument(argument);
        const auto result=CaPlane::RunGame(root,command);
        Check(result.error==ERROR_SUCCESS && result.exitCode==expected,"actual game exit code, arguments and installed working directory");
    }
    fs::remove(runtime);
    const auto missing=CaPlane::RunGame(root,CaPlane::QuoteArgument(runtime.wstring()));
    Check(missing.error==ERROR_FILE_NOT_FOUND,"missing runtime cannot silently fall back to a successful bootstrap");
    fs::remove(root/L"SimPlaneurUE.exe");
    fs::remove(runtime.parent_path());fs::remove(runtime.parent_path().parent_path());
    fs::remove(runtime.parent_path().parent_path().parent_path());fs::remove(root);
    return failures?1:0;
}
