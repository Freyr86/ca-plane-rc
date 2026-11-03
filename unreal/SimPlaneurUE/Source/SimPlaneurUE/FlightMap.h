#pragma once
#include "Widgets/SLeafWidget.h"
#include "Slate/DeferredCleanupSlateBrush.h"

class ASlopeFlightPawn;

class SFlightMap : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SFlightMap) {} SLATE_ARGUMENT(ASlopeFlightPawn*, Plane) SLATE_END_ARGS()
    void Construct(const FArguments& Args);
    void Zoom(double Factor, FVector2D Anchor);
    void Recenter();
    FString Coordinates() const;
    FVector2D ComputeDesiredSize(float) const override { return {660,335}; }
    int32 OnPaint(const FPaintArgs&,const FGeometry&,const FSlateRect&,FSlateWindowElementList&,int32,const FWidgetStyle&,bool) const override;
    FReply OnMouseWheel(const FGeometry&,const FPointerEvent&) override;
    FReply OnMouseButtonDown(const FGeometry&,const FPointerEvent&) override;
    FReply OnMouseButtonUp(const FGeometry&,const FPointerEvent&) override;
    FReply OnMouseMove(const FGeometry&,const FPointerEvent&) override;
private:
    TWeakObjectPtr<ASlopeFlightPawn> Plane;
    TSharedPtr<FDeferredCleanupSlateBrush> Massif, Lake;
    FVector2D Centre, CursorLocal, DragStart;
    bool bHasCursor=false;
    double Span=3000.; // Width in metres. World X points north, Y east.
    FVector2D WorldAt(FVector2D Local) const;
    void ClampView();
};
