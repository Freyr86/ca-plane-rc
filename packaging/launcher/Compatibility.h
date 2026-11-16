#pragma once
#include <string>
#include <vector>

namespace CaPlane
{
struct Adapter
{
    std::wstring name, driver;
    unsigned index = 0;
    unsigned long long dedicatedMB = 0, sharedMB = 0;
    bool dx12 = false, dx11 = false;
};
struct Machine
{
    unsigned windowsMajor = 0, windowsBuild = 0;
    unsigned long long ramMB = 0, availableMB = 0;
    bool sse42 = false;
    std::wstring cpu;
    std::vector<Adapter> adapters;
};
struct Assessment
{
    bool blocked = false, recommendLow = false;
    int normalAdapter = -1, fallbackAdapter = -1;
    std::vector<std::wstring> errors, warnings;
};
inline Assessment Assess(const Machine& m)
{
    Assessment a;
    if (m.windowsMajor < 10) a.errors.push_back(L"Windows 10 ou 11 en 64 bits est nécessaire.");
    else if (m.windowsBuild < 19045) a.warnings.push_back(L"Cette version de Windows est ancienne. Mettre Windows à jour avant de réessayer en cas de problème.");
    if (!m.sse42) a.errors.push_back(L"Le processeur ne prend pas en charge SSE4.2, nécessaire au moteur du simulateur.");
    auto score=[](const Adapter& g) {return g.dedicatedMB;};
    for (int i=0;i<int(m.adapters.size());++i)
    {
        const auto& g=m.adapters[i];
        if (g.dx12 && (a.normalAdapter<0 || score(g)>score(m.adapters[a.normalAdapter]))) a.normalAdapter=i;
        if (g.dx11 && (a.fallbackAdapter<0 || score(g)>score(m.adapters[a.fallbackAdapter]))) a.fallbackAdapter=i;
    }
    if (a.normalAdapter<0 && a.fallbackAdapter<0)
        a.errors.push_back(L"Aucune carte graphique utilisable détectée : DirectX 11 ou 12 avec niveau de fonctionnalités 11.0 minimum requis. Vérifier le pilote graphique.");
    else if (a.normalAdapter<0)
        a.warnings.push_back(L"DirectX 12 indisponible. Essayer le mode de compatibilité DirectX 11, avec graphismes réduits.");
    const int selected=a.normalAdapter>=0?a.normalAdapter:a.fallbackAdapter;
    if (selected>=0 && m.adapters[selected].dedicatedMB<3072)
        a.warnings.push_back(L"Mémoire graphique dédiée limitée ou partagée : graphismes réduits conseillés. La fluidité dépend de la carte et de la scène.");
    if (m.ramMB<8192) a.warnings.push_back(L"Moins de 8 Go de mémoire vive détectés : risque de lenteurs ou de manque de mémoire.");
    if (m.availableMB<2048) a.warnings.push_back(L"Moins de 2 Go de mémoire vive disponible. Fermer les applications inutilisées avant le lancement.");
    a.blocked=!a.errors.empty();
    a.recommendLow=!a.warnings.empty();
    return a;
}
}
