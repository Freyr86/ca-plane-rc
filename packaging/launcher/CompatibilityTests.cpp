#include "Compatibility.h"
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
    std::cout << "COMPATIBILITY_TESTS passed (DX12, DX11-only, unsupported, hybrid GPUs, CPU, OS, memory)\n";
}
