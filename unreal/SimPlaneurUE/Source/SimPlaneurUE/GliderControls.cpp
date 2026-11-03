#include "GliderControls.h"

namespace
{
    using G=EControlGroup;
    using T=EControlType;
    constexpr FControlSpec Controls[]={
        {TEXT("Rudder"),TEXT("Direction"),G::Pilot,T::Signed,true,TEXT("D"),TEXT("A")},
        {TEXT("Elevator"),TEXT("Profondeur"),G::Pilot,T::Signed,true,TEXT("Up"),TEXT("Down")},
        {TEXT("Aileron"),TEXT("Ailerons"),G::Pilot,T::Signed,false,TEXT("Right"),TEXT("Left")},
        {TEXT("LowRates"),TEXT("Petits débattements"),G::Pilot,T::Switch,true},
        {TEXT("TrimRudder"),TEXT("Trim de direction"),G::Trim,T::Trim,true},
        {TEXT("TrimElevator"),TEXT("Trim de profondeur"),G::Trim,T::Trim,true},
        {TEXT("TrimAileron"),TEXT("Trim des ailerons"),G::Trim,T::Trim,false},
        {TEXT("Flaps"),TEXT("Volets"),G::Wing,T::CenteredRange,false,TEXT("PageUp"),TEXT("PageDown")},
        {TEXT("Spoilers"),TEXT("Aérofreins"),G::Wing,T::Range,false},
        {TEXT("Butterfly"),TEXT("Crocodile / butterfly"),G::Wing,T::Range,false},
        {TEXT("Motor"),TEXT("Gaz moteur"),G::Motor,T::Range,false},
        {TEXT("MotorCut"),TEXT("Coupure moteur"),G::Motor,T::Switch,false},
        {TEXT("Pylon"),TEXT("Pylône rétractable"),G::Motor,T::Switch,false},
        {TEXT("Gear"),TEXT("Train rentrant"),G::Equipment,T::Switch,false},
        {TEXT("WheelBrake"),TEXT("Frein de roue"),G::Equipment,T::Range,false},
        {TEXT("TowRelease"),TEXT("Largage du crochet"),G::Equipment,T::Pulse,false},
        {TEXT("Ballast"),TEXT("Vidange du ballast"),G::Equipment,T::Switch,false},
        {TEXT("Relaunch"),TEXT("Remettre le planeur en main"),G::Simulator,T::Pulse,true,TEXT("R")},
        {TEXT("Camera"),TEXT("Changer de caméra"),G::Simulator,T::Pulse,true,TEXT("C")},
        {TEXT("Instruments"),TEXT("Afficher les instruments"),G::Simulator,T::Pulse,true,TEXT("H")},
        {TEXT("Menu"),TEXT("Ouvrir le menu"),G::Simulator,T::Pulse,true},
        {TEXT("Meadow"),TEXT("Observer la prairie"),G::Simulator,T::Pulse,true,TEXT("V")},
        {TEXT("Sidestep"),TEXT("Déplacer le pilote"),G::Simulator,T::Signed,true,TEXT("L"),TEXT("J")},
        {TEXT("Aux1"),TEXT("Voie auxiliaire 1"),G::Aux,T::Signed,false},
        {TEXT("Aux2"),TEXT("Voie auxiliaire 2"),G::Aux,T::Signed,false},
        {TEXT("Aux3"),TEXT("Voie auxiliaire 3"),G::Aux,T::Signed,false},
        {TEXT("Aux4"),TEXT("Voie auxiliaire 4"),G::Aux,T::Signed,false},
        {TEXT("WindTrails"),TEXT("Afficher les filets d’air"),G::Simulator,T::Pulse,true,TEXT("F")},
        {TEXT("HandLaunch"),TEXT("Lancer à la main"),G::Simulator,T::Pulse,true,TEXT("SpaceBar")},
        {TEXT("Noclip"),TEXT("Explorer en noclip"),G::Simulator,T::Pulse,true,TEXT("N")},
        {TEXT("Vario"),TEXT("Activer le vario sonore"),G::Equipment,T::Pulse,true,TEXT("K")}
    };
    static_assert(UE_ARRAY_COUNT(Controls)==ControlCount);
}
const FControlSpec& ControlSpec(EGliderControl A) {return Controls[int32(A)];}
const TCHAR* GroupLabel(EControlGroup G)
{
    static const TCHAR* Names[]={TEXT("Pilotage"),TEXT("Trims"),TEXT("Aile"),TEXT("Motorisation"),
        TEXT("Équipement"),TEXT("Simulateur"),TEXT("Auxiliaires")};
    return Names[int32(G)];
}
