#pragma once
#include <windows.h>

namespace CaPlane
{
struct RuntimeVersion
{
    bool installed=false;
    DWORD major=0,minor=0,build=0;
};

// The game and this launcher use the same MSVC toolchain. A later compatible
// runtime must never be downgraded to the bundled redistributable.
inline bool RuntimeVersionSupported(const RuntimeVersion& v)
{
    if(!v.installed) return false;
    if(v.major!=14) return v.major>14;
    if(v.minor!=_MSC_VER%100) return v.minor>_MSC_VER%100;
    return v.build>=_MSC_FULL_VER%100000;
}

inline bool RuntimeReady()
{
    constexpr auto key=L"SOFTWARE\\Microsoft\\VisualStudio\\14.0\\VC\\Runtimes\\x64";
    auto Read=[&](const wchar_t* name)
    {
        DWORD value=0,bytes=sizeof(value);
        if(RegGetValueW(HKEY_LOCAL_MACHINE,key,name,RRF_RT_REG_DWORD|RRF_SUBKEY_WOW6464KEY,nullptr,&value,&bytes)!=ERROR_SUCCESS) return DWORD(0);
        return value;
    };
    if(!RuntimeVersionSupported({Read(L"Installed")==1,Read(L"Major"),Read(L"Minor"),Read(L"Bld")})) return false;
    // The registry can survive a damaged installation. Verify the actual x64
    // libraries in System32, without searching the current working directory.
    for(const auto* name:{L"vcruntime140.dll",L"vcruntime140_1.dll",L"msvcp140.dll"})
    {
        const HMODULE library=LoadLibraryExW(name,nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
        if(!library) return false;
        FreeLibrary(library);
    }
    return true;
}

enum class RuntimeAction { Continue, OfferInstaller, ReportMissing, StopUnattended };
inline RuntimeAction RuntimeNextAction(bool ready,bool unattended,bool installerPresent)
{
    if(ready) return RuntimeAction::Continue;
    if(unattended) return RuntimeAction::StopUnattended;
    return installerPresent?RuntimeAction::OfferInstaller:RuntimeAction::ReportMissing;
}
}
