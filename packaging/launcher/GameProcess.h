#pragma once
#include <windows.h>
#include <filesystem>
#include <string>

namespace CaPlane
{
inline std::filesystem::path GameExecutable(const std::filesystem::path& root)
{
    // The root executable is Unreal's prerequisite bootstrap. Its success does
    // not mean the actual game exited successfully, so monitor the runtime.
    return root/L"SimPlaneurUE"/L"Binaries"/L"Win64"/L"SimPlaneurUE.exe";
}

inline std::wstring QuoteArgument(const std::wstring& s)
{
    // Windows CommandLineToArgvW escaping, including trailing backslashes.
    std::wstring out=L"\""; size_t slash=0;
    for(wchar_t c:s) {if(c==L'\\') {++slash;continue;} out.append(slash*(c==L'"'?2:1),L'\\'); slash=0; if(c==L'"') out+=L'\\';out+=c;}
    out.append(slash*2,L'\\'); return out+L'"';
}

struct ProcessResult
{
    DWORD exitCode=0;
    DWORD error=ERROR_SUCCESS;
};

inline ProcessResult RunGame(const std::filesystem::path& root,std::wstring command)
{
    const auto game=GameExecutable(root);
    STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION child{};
    if(!CreateProcessW(game.c_str(),command.data(),nullptr,nullptr,FALSE,0,nullptr,root.c_str(),&startup,&child))
        return {0,GetLastError()};
    CloseHandle(child.hThread);
    ProcessResult result;
    if(WaitForSingleObject(child.hProcess,INFINITE)==WAIT_FAILED) result.error=GetLastError();
    else if(!GetExitCodeProcess(child.hProcess,&result.exitCode)) result.error=GetLastError();
    CloseHandle(child.hProcess);
    return result;
}
}
