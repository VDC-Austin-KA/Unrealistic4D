// Copyright Earth4D. Licensed for project use.
// The 4D timeline: a day ruler, stage lanes, draggable/resizable task bars, and a
// scrubbable playhead — the "spine" of the tool (PRODUCT.md design principle 2).
// Entirely custom-painted so it can match the cinematic dark design language and
// translate the web app's Gantt 1:1. Every mutation routes through UEarth4DSubsystem.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Earth4DTypes.h"

class UEarth4DSubsystem;

DECLARE_DELEGATE_OneParam(FOnEarth4DTaskSelected, FString /*TaskId*/);

class SEarth4DTimeline : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SEarth4DTimeline) {}
		/** Resolves the live command layer (re-resolved each frame; world can change). */
		SLATE_ARGUMENT(TFunction<UEarth4DSubsystem*()>, SubsystemGetter)
		/** Fired when a task bar is clicked (empty string = cleared). */
		SLATE_EVENT(FOnEarth4DTaskSelected, OnTaskSelected)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Currently selected task id (drives the inspector + bar highlight). */
	const FString& GetSelectedTaskId() const { return SelectedTaskId; }
	void SetSelectedTaskId(const FString& InId) { SelectedTaskId = InId; }

	// SWidget
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override;
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(720.f, 280.f); }
	virtual bool SupportsKeyboardFocus() const override { return true; }

private:
	// ---- layout metrics (recomputed from the local geometry each use) ----
	struct FMetrics
	{
		float GutterWidth = 168.f;   // task-name column on the left
		float RulerHeight = 26.f;    // day-ruler strip at the top
		float RowHeight = 26.f;      // one task per row
		float RowPad = 3.f;          // gap between bar and row edges
		float TrackLeft = 0.f;       // = GutterWidth
		float TrackWidth = 0.f;      // pixels available for the day track
		float MinDay = 0.f;          // first visible day
		float MaxDay = 1.f;          // last visible day
		float PixelsPerDay = 1.f;
		float DayToX(float Day) const { return TrackLeft + (Day - MinDay) * PixelsPerDay; }
		float XToDay(float X) const { return MinDay + (X - TrackLeft) / FMath::Max(PixelsPerDay, KINDA_SMALL_NUMBER); }
	};
	FMetrics ComputeMetrics(const FVector2D& LocalSize, UEarth4DSubsystem* Sub) const;

	enum class EDragMode : uint8 { None, MoveBar, ResizeStart, ResizeEnd, Scrub };

	UEarth4DSubsystem* ResolveSub() const { return SubsystemGetter ? SubsystemGetter() : nullptr; }
	int32 RowIndexAt(const FVector2D& Local, const FMetrics& M) const; // task row under a local point, or INDEX_NONE
	bool HitTestBar(const FVector2D& Local, const FMetrics& M, FString& OutTaskId, EDragMode& OutMode) const;

	TFunction<UEarth4DSubsystem*()> SubsystemGetter;
	FOnEarth4DTaskSelected OnTaskSelected;

	FString SelectedTaskId;

	// Drag state
	EDragMode DragMode = EDragMode::None;
	FString DragTaskId;
	float DragGrabDayOffset = 0.f;  // (grab day) - (task start) so the bar doesn't jump
	float ViewMinDay = 0.f;         // horizontal scroll/zoom (zoom via wheel)
	float ZoomDaysPerView = -1.f;   // <0 = auto-fit to schedule bounds
};
