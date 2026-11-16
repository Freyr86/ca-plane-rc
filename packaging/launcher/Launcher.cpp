#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <dxgi1_6.h>
#include <d3d11.h>
#include <d3d12.h>
#include <wrl/client.h>
#include <intrin.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include "Compatibility.h"
#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")

using Microsoft::WRL::ComPtr;
namespace fs=std::filesystem;
namespace
{
const wchar_t* Support=L"https://ca-plane-assistance.endlessbow16.chatgpt.site/";
std::string Utf8(const std::wstring& s)
{
    if(s.empty()) return {};
    int n=WideCharToMultiByte(CP_UTF8,0,s.data(),int(s.size()),nullptr,0,nullptr,nullptr);
    std::string r(n,'\0'); WideCharToMultiByte(CP_UTF8,0,s.data(),int(s.size()),r.data(),n,nullptr,nullptr); return r;
}
std::wstring Wide(const std::string& s)
{
    if(s.empty()) return {};
    int n=MultiByteToWideChar(CP_UTF8,0,s.data(),int(s.size()),nullptr,0);
    std::wstring r(n,L'\0'); MultiByteToWideChar(CP_UTF8,0,s.data(),int(s.size()),r.data(),n); return r;
}
std::wstring Encode(const std::string& s)
{
    std::wostringstream o;
    for(unsigned char c:s) if((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.'||c=='~') o<<wchar_t(c);
    else o<<L'%'<<std::uppercase<<std::hex<<std::setw(2)<<std::setfill(L'0')<<int(c);
    return o.str();
}
std::wstring Quote(const std::wstring& s)
{
    // Windows CommandLineToArgvW escaping, including trailing backslashes.
    std::wstring out=L"\""; size_t slash=0;
    for(wchar_t c:s) {if(c==L'\\') {++slash;continue;} out.append(slash*(c==L'"'?2:1),L'\\'); slash=0; if(c==L'"') out+=L'\\';out+=c;}
    out.append(slash*2,L'\\'); return out+L'"';
}
std::wstring Json(const std::wstring& s)
{
    std::wstring o=L"\"";
    for(wchar_t c:s) {if(c==L'"'||c==L'\\') o+=L'\\'; if(c>=32) o+=c; else if(c==L'\n') o+=L"\\n";}
    return o+L'"';
}
CaPlane::Machine Inspect()
{
    CaPlane::Machine m;
    OSVERSIONINFOW os{};os.dwOSVersionInfoSize=sizeof(os);
    using RtlVersion=LONG(WINAPI*)(OSVERSIONINFOW*);
    if(auto fn=reinterpret_cast<RtlVersion>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"RtlGetVersion")); fn&&fn(&os)==0)
    {m.windowsMajor=os.dwMajorVersion;m.windowsBuild=os.dwBuildNumber;}
    int cpu[4]{}; __cpuid(cpu,1);m.sse42=(cpu[2]&(1<<20))!=0;
    char brand[49]{}; __cpuid(cpu,0x80000000);
    if(static_cast<unsigned>(cpu[0])>=0x80000004u) for(int i=0;i<3;++i) {__cpuid(cpu,0x80000002+i);memcpy(brand+i*16,cpu,16);}
    m.cpu=Wide(brand);
    MEMORYSTATUSEX mem{};mem.dwLength=sizeof(mem);
    if(GlobalMemoryStatusEx(&mem)) {m.ramMB=mem.ullTotalPhys/(1024*1024);m.availableMB=mem.ullAvailPhys/(1024*1024);}
    // Load DX12 dynamically: even a missing runtime must produce our explanation.
    HMODULE dx12=LoadLibraryExW(L"d3d12.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
    using Create12=HRESULT(WINAPI*)(IUnknown*,D3D_FEATURE_LEVEL,REFIID,void**);
    auto create12=dx12?reinterpret_cast<Create12>(GetProcAddress(dx12,"D3D12CreateDevice")):nullptr;
    ComPtr<IDXGIFactory1> factory;
    if(SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))))
    {
        ComPtr<IDXGIFactory6> factory6; factory.As(&factory6);
        for(UINT i=0;;++i)
        {
            ComPtr<IDXGIAdapter1> adapter;
            HRESULT hr=factory6?factory6->EnumAdapterByGpuPreference(i,DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,IID_PPV_ARGS(&adapter)):factory->EnumAdapters1(i,&adapter);
            if(hr==DXGI_ERROR_NOT_FOUND) break; if(FAILED(hr)) break;
            DXGI_ADAPTER_DESC1 d{}; if(FAILED(adapter->GetDesc1(&d))||(d.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)) continue;
            CaPlane::Adapter g;g.index=i;g.name=d.Description;g.dedicatedMB=d.DedicatedVideoMemory/(1024*1024);g.sharedMB=d.SharedSystemMemory/(1024*1024);
            LARGE_INTEGER driver{};
            if(SUCCEEDED(adapter->CheckInterfaceSupport(__uuidof(IDXGIDevice),&driver)))
                g.driver=std::to_wstring(HIWORD(driver.HighPart))+L"."+std::to_wstring(LOWORD(driver.HighPart))+L"."+std::to_wstring(HIWORD(driver.LowPart))+L"."+std::to_wstring(LOWORD(driver.LowPart));
            ComPtr<ID3D12Device> device12;
            // This project ships SM5 shaders. Do not require SM6/RT on an SM5 build.
            g.dx12=create12&&SUCCEEDED(create12(adapter.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device12)));
            ComPtr<ID3D11Device> device11; D3D_FEATURE_LEVEL level{};
            const D3D_FEATURE_LEVEL levels[]={D3D_FEATURE_LEVEL_11_0};
            g.dx11=SUCCEEDED(D3D11CreateDevice(adapter.Get(),D3D_DRIVER_TYPE_UNKNOWN,nullptr,0,levels,1,D3D11_SDK_VERSION,&device11,&level,nullptr));
            m.adapters.push_back(g);
        }
    }
    if(dx12) FreeLibrary(dx12);
    return m;
}
fs::path Root()
{
    wchar_t path[32768]{};GetModuleFileNameW(nullptr,path,DWORD(std::size(path)));return fs::path(path).parent_path();
}
std::wstring Version(const fs::path& root)
{
    std::ifstream f(root/L"CaPlaneRC-version.txt");std::string v; std::getline(f,v);
    if(v.size()>30||v.find_first_not_of("0123456789.\r")!=std::string::npos) return L"inconnue";
    while(!v.empty()&&v.back()=='\r')v.pop_back();return v.empty()?L"inconnue":Wide(v);
}
std::wstring Summary(const CaPlane::Machine& m,const std::wstring& version)
{
    std::wostringstream o;o<<L"Ça plane ! "<<version<<L"\r\nWindows "<<(m.windowsBuild>=22000?11:m.windowsMajor)<<L" — build "<<m.windowsBuild<<L" (64 bits)\r\n"<<m.cpu<<L"\r\nMémoire vive : "<<(m.ramMB+512)/1024<<L" Go ; disponible : "<<m.availableMB/1024<<L" Go\r\n";
    for(const auto& g:m.adapters) o<<L"\r\n"<<g.name<<L"\r\nPilote : "<<g.driver<<L"\r\nMémoire dédiée : "<<g.dedicatedMB<<L" Mo ; partagée : "<<g.sharedMB<<L" Mo\r\nDirectX 12 : "<<(g.dx12?L"oui":L"non")<<L" ; DirectX 11 : "<<(g.dx11?L"oui":L"non")<<L"\r\n";
    return o.str();
}
std::wstring Payload(const CaPlane::Machine& m,const CaPlane::Assessment& a,const std::wstring& version,DWORD exitCode)
{
    std::wstring gpu,drivers;
    for(const auto& g:m.adapters) {if(!gpu.empty()){gpu+=L" / ";drivers+=L" / ";} gpu+=g.name;drivers+=g.driver;}
    std::wstring warnings;for(const auto& s:a.errors) warnings+=s+L" ";for(const auto& s:a.warnings) warnings+=s+L" ";
    return L"{\"version\":"+Json(version)+L",\"os\":"+Json(L"Windows "+std::to_wstring(m.windowsBuild>=22000?11:m.windowsMajor)+L" build "+std::to_wstring(m.windowsBuild)+L" x64")+
        L",\"cpu\":"+Json(m.cpu)+L",\"gpu\":"+Json(gpu)+L",\"driver\":"+Json(drivers)+L",\"ramMB\":"+std::to_wstring(m.ramMB)+
        L",\"dx12\":"+(a.normalAdapter>=0?L"true":L"false")+L",\"dx11\":"+(a.fallbackAdapter>=0?L"true":L"false")+
        L",\"exitCode\":"+std::to_wstring(exitCode)+L",\"compatibility\":"+Json(warnings)+L"}";
}
void Report(const CaPlane::Machine& m,const CaPlane::Assessment& a,const std::wstring& version,DWORD code)
{
    const auto url=std::wstring(Support)+L"#diagnostic="+Encode(Utf8(Payload(m,a,version,code)));
    // Fragment is read locally by the form; nothing is submitted until the player confirms.
    if(INT_PTR(ShellExecuteW(nullptr,L"open",url.c_str(),nullptr,nullptr,SW_SHOWNORMAL))<=32)
        MessageBoxW(nullptr,L"Impossible d’ouvrir le navigateur. Le formulaire est disponible sur ca-plane-assistance.endlessbow16.chatgpt.site.",L"Ça plane ! — Signaler un problème",MB_OK|MB_ICONINFORMATION);
}
void SaveDiagnostic(const std::wstring& text)
{
    PWSTR local=nullptr;if(FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData,0,nullptr,&local)))return;
    const fs::path dir=fs::path(local)/L"CaPlaneRC"/L"Diagnostics";CoTaskMemFree(local);
    std::error_code ec;fs::create_directories(dir,ec);if(ec)return;
    std::ofstream file(dir/L"compatibility.txt",std::ios::binary);file<<Utf8(text);
}
int Dialog(const std::wstring& title,const std::wstring& content,const std::wstring& detail,const std::vector<TASKDIALOG_BUTTON>& buttons,int def)
{
    TASKDIALOGCONFIG c{};c.cbSize=sizeof(c);c.dwFlags=TDF_SIZE_TO_CONTENT|TDF_ALLOW_DIALOG_CANCELLATION;c.pszWindowTitle=L"Ça plane ! — Avant le départ";
    c.pszMainInstruction=title.c_str();c.pszContent=content.c_str();c.pszExpandedInformation=detail.c_str();c.pszExpandedControlText=L"Masquer la configuration";c.pszCollapsedControlText=L"Voir la configuration";
    c.pszMainIcon=TD_INFORMATION_ICON;c.cButtons=UINT(buttons.size());c.pButtons=buttons.data();c.nDefaultButton=def;
    int result=IDCANCEL;if(FAILED(TaskDialogIndirect(&c,&result,nullptr,nullptr))) MessageBoxW(nullptr,content.c_str(),title.c_str(),MB_OK|MB_ICONWARNING);return result;
}
}

int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int)
{
    INITCOMMONCONTROLSEX controls{sizeof(controls),ICC_STANDARD_CLASSES};InitCommonControlsEx(&controls);
    auto dpi=reinterpret_cast<BOOL(WINAPI*)()>(GetProcAddress(GetModuleHandleW(L"user32.dll"),"SetProcessDPIAware"));if(dpi)dpi();
    int argc=0;LPWSTR* argv=CommandLineToArgvW(GetCommandLineW(),&argc);
    bool report=false,check=false,quiet=false;std::vector<std::wstring> forward;
    for(int i=1;i<argc;++i) {std::wstring v=argv[i];if(v==L"--report")report=true;else if(v==L"--check")check=true;else {forward.push_back(v);if(v==L"-unattended")quiet=true;}}
    if(argv) LocalFree(argv);
    const auto root=Root();const auto version=Version(root);const auto m=Inspect();const auto a=CaPlane::Assess(m);const auto detail=Summary(m,version);
    SaveDiagnostic(detail);
    if(report) {Report(m,a,version,0);return 0;}
    if(check) {std::ofstream(root/L"compatibility-check.json",std::ios::binary)<<Utf8(Payload(m,a,version,0));return a.blocked?2:0;}
    const auto game=root/L"SimPlaneurUE.exe";
    if(!fs::exists(game)) {if(!quiet) MessageBoxW(nullptr,L"Le jeu est absent de ce dossier. Réinstaller Ça plane ! à partir de l’installateur officiel.",L"Installation incomplète",MB_OK|MB_ICONERROR);return 3;}
    bool low=false;
    if(!quiet)
    {
        std::wstring content;
        for(const auto& s:a.errors) content+=L"• "+s+L"\n\n";
        for(const auto& s:a.warnings) content+=L"• "+s+L"\n\n";
        if(content.empty())content=L"Les fonctions nécessaires au lancement sont disponibles. Ce contrôle ne garantit pas la fluidité en jeu.";
        std::vector<TASKDIALOG_BUTTON> b;
        if(!a.blocked&&a.normalAdapter>=0)b.push_back({100,L"Lancer en DirectX 12"});
        if(!a.blocked&&a.fallbackAdapter>=0)b.push_back({101,L"DirectX 11 · graphismes réduits"});
        b.push_back({102,L"Signaler un problème"});b.push_back({IDCANCEL,L"Fermer"});
        int choice=Dialog(a.blocked?L"Cette configuration ne peut pas démarrer le jeu":a.recommendLow?L"Vérifier ces points avant de voler":L"Configuration vérifiée",content,detail,b,a.blocked?102:a.recommendLow&&a.fallbackAdapter>=0?101:100);
        if(choice==102){Report(m,a,version,0);return 0;}if(choice!=100&&choice!=101)return 0;low=choice==101;
    }
    else {if(a.blocked)return 2;low=a.normalAdapter<0;}
    std::wstring cmd=Quote(game.wstring());
    // Preserve all test/user arguments; never silently bypass a requested RHI.
    bool explicitRhi=false;for(const auto& v:forward)if(v==L"-dx11"||v==L"-d3d11"||v==L"-dx12"||v==L"-d3d12"||v==L"-vulkan"||v==L"-nullrhi"||v==L"-NullRHI")explicitRhi=true;
    if(!explicitRhi)
    {
        const int index=low?a.fallbackAdapter:a.normalAdapter;
        cmd+=low?L" -dx11 -CaPlaneLowSpec -windowed -ResX=1280 -ResY=720":L" -dx12";
        // Match Unreal's explicit high-performance enumeration. Default DXGI
        // order may differ between this executable and the game's GPU exports.
        if(index>=0)cmd+=L" -gpupreference=2 -graphicsadapter="+std::to_wstring(m.adapters[index].index);
    }
    for(const auto& v:forward)cmd+=L" "+Quote(v);
    STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION child{};
    if(!CreateProcessW(game.c_str(),cmd.data(),nullptr,nullptr,FALSE,0,nullptr,root.c_str(),&startup,&child))
    {
        DWORD error=GetLastError();if(!quiet){Dialog(L"Le jeu n’a pas pu démarrer",L"Vérifier l’installation et les prérequis Windows. Code : "+std::to_wstring(error),detail,{{102,L"Signaler un problème"},{IDCANCEL,L"Fermer"}},102)==102?Report(m,a,version,error):void();}return int(error);
    }
    CloseHandle(child.hThread);WaitForSingleObject(child.hProcess,INFINITE);DWORD code=0;GetExitCodeProcess(child.hProcess,&code);CloseHandle(child.hProcess);
    if(code!=0&&!quiet)
        if(Dialog(L"Le simulateur s’est arrêté avec une erreur",L"Tu peux envoyer un rapport sans compte. Le formulaire te permettra de vérifier les informations avant l’envoi. Code : "+std::to_wstring(code),detail,{{102,L"Préparer un rapport"},{IDCANCEL,L"Fermer"}},102)==102) Report(m,a,version,code);
    return int(code);
}
