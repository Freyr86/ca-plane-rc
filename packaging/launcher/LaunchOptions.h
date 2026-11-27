#pragma once
#include "Compatibility.h"
#include <cwctype>

namespace CaPlane
{
// Keep renderer selection separate from the recovery profile: an explicit
// -dx11 used to bypass both the reduced graphics and the preferred adapter.
inline std::vector<std::wstring> LaunchOptions(const Machine& m,const Assessment& a,
    bool low,const std::vector<std::wstring>& forward)
{
    bool explicitRhi=false,dx11=false,dx12=false,explicitAdapter=false;
    for(auto v:forward)
    {
        for(auto& c:v)c=wchar_t(std::towlower(c));
        dx11|=v==L"-dx11"||v==L"-d3d11";
        dx12|=v==L"-dx12"||v==L"-d3d12";
        explicitRhi|=dx11||dx12||v==L"-vulkan"||v==L"-nullrhi";
        explicitAdapter|=v.rfind(L"-graphicsadapter=",0)==0;
        low|=v==L"-caplanelowspec";
    }
    low|=dx11;
    std::vector<std::wstring> options;
    if(!explicitRhi) options.push_back(low?L"-dx11":L"-dx12");
    if(!explicitRhi||dx11||dx12)
    {
        if(low)
            options.insert(options.end(),{L"-CaPlaneLowSpec",L"-windowed",L"-ResX=1280",L"-ResY=720"});
        const int index=dx11?a.fallbackAdapter:dx12?a.normalAdapter:low?a.fallbackAdapter:a.normalAdapter;
        // Match Unreal's explicit high-performance enumeration, including an
        // explicitly requested DirectX renderer. Preserve a user-selected GPU.
        if(!explicitAdapter&&index>=0)
            options.insert(options.end(),{L"-gpupreference=2",L"-graphicsadapter="+std::to_wstring(m.adapters[index].index)});
    }
    options.insert(options.end(),forward.begin(),forward.end());
    return options;
}
}
