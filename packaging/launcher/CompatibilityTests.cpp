#include "Compatibility.h"
#include "LaunchOptions.h"
#include <algorithm>
#include <cassert>
#include <iostream>
using namespace CaPlane;
int main()
{
    Machine m{10,22631,16384,10000,true,L"CPU",{{L"GPU",L"driver",0,8192,8000,true,true}}};
    auto a=Assess(m); assert(!a.blocked && !a.recommendLow && a.normalAdapter==0);
    // Intel HD 4600 class: loss of DX12 must allow a DX11 attempt, not a false pass.
    m.adapters[0]={L"Intel HD 4600",L"driver",0,128,8000,false,true};
    a=Assess(m); assert(!a.blocked && a.recommendLow && a.normalAdapter==-1 && a.fallbackAdapter==0);
    m.adapters[0].dx11=false; assert(Assess(m).blocked);
    m.adapters.push_back({L"Discrete GPU",L"driver",1,8192,8000,true,true});
    a=Assess(m); assert(!a.blocked && a.normalAdapter==1 && a.fallbackAdapter==1);
    m.sse42=false; assert(Assess(m).blocked); m.sse42=true;
    m.windowsMajor=6; assert(Assess(m).blocked); m.windowsMajor=10;
    m.ramMB=4096; m.availableMB=900; a=Assess(m); assert(!a.blocked && a.recommendLow && a.warnings.size()==2);
    m.adapters.clear(); assert(Assess(m).blocked);
    // Reported machine: prefer the Radeon, not the Intel shared-memory GPU.
    m={10,19045,8069,4000,true,L"i5-4440",{
        {L"Intel HD 4600",L"driver",1,113,4000,false,true},
        {L"AMD Radeon R9 200 Series",L"driver",0,2048,4000,true,true}}};
    a=Assess(m); assert(!a.blocked && a.recommendLow && a.normalAdapter==1 && a.fallbackAdapter==1);
    auto has=[](const auto& args,const wchar_t* value){return std::find(args.begin(),args.end(),value)!=args.end();};
    auto args=LaunchOptions(m,a,true,{});
    assert(has(args,L"-dx11") && has(args,L"-CaPlaneLowSpec") && has(args,L"-graphicsadapter=0"));
    for(const wchar_t* rhi:{L"-dx11",L"-d3d11",L"-DX11"})
    {
        args=LaunchOptions(m,a,false,{rhi,L"-unattended"});
        assert(has(args,L"-CaPlaneLowSpec") && has(args,L"-windowed") && has(args,L"-ResX=1280")
            && has(args,L"-graphicsadapter=0") && has(args,L"-unattended") && !has(args,L"-dx12"));
    }
    args=LaunchOptions(m,a,false,{});
    assert(has(args,L"-dx12") && !has(args,L"-CaPlaneLowSpec"));
    args=LaunchOptions(m,a,false,{L"-CaPlaneLowSpec"});
    assert(has(args,L"-dx11") && has(args,L"-windowed"));
    args=LaunchOptions(m,a,true,{L"-D3D12"});
    assert(has(args,L"-D3D12") && !has(args,L"-dx11") && has(args,L"-CaPlaneLowSpec"));
    args=LaunchOptions(m,a,true,{L"-NullRHI"});
    assert(args==std::vector<std::wstring>{L"-NullRHI"});
    args=LaunchOptions(m,a,true,{L"-dx11",L"-graphicsadapter=1"});
    assert(has(args,L"-graphicsadapter=1") && !has(args,L"-graphicsadapter=0"));
    std::cout << "LAUNCH_OPTIONS_TESTS passed (R9 200 + HD 4600, explicit DX11, recovery, DX12, NullRHI, GPU override)\n";
    std::cout << "COMPATIBILITY_TESTS passed (DX12, DX11-only, unsupported, hybrid GPUs, CPU, OS, memory)\n";
}
