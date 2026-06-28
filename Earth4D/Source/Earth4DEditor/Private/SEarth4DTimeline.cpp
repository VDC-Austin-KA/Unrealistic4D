// Copyright Earth4D. Licensed for project use.
#include "SEarth4DTimeline.h"
#include "Earth4DSubsystem.h"
#include "Earth4DSchedule.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"

#define LOCTEXT_NAMESPACE "Earth4D"

namespace
{
	// Dependency-free tintable fill used for every rect/line.
	static const FSlateColorBrush GWhiteBrush(FLinearColor::White);

	// Cinematic dark palette (kept in lockstep with the web design system).
	const FLinearColor C_PanelBg(0.043f, 0.051f, 0.063f);
	const FLinearColor C_RulerBg(0.078f, 0.090f, 0.106f);
	const FLinearColor C_Grid(1.f, 1.f, 1.f, 0.055f);
	const FLinearColor C_GridStrong(1.f, 1.f, 1.f, 0.11f);
	const FLinearColor C_RowAlt(1.f, 1.f, 1.f, 0.018f);
	const FLinearColor C_Text(0.78f, 0.82f, 0.88f);
	const FLinearColor C_TextDim(0.5f, 0.55f, 0.62f);
	const FLinearColor C_Playhead(0.97f, 0.83f, 0.30f);
	const FLinearColor C_Select(1.f, 1.f, 1.f, 0.95f);

	FLinearColor TypeColor(EEarth4DTaskType Type)
	{
		switch (Type)
		{
		case EEarth4DTaskType::Demolition: return FLinearColor(0.85f, 0.27f, 0.22f);
		case EEarth4DTaskType::Temporary:  return FLinearColor(0.90f, 0.70f, 0.22f);
		default:                           return FLinearColor(0.22f, 0.56f, 0.92f);
		}
	}

	float NiceStep(float ApproxDaysPerLabel)
	{
		const float Pow10 = FMath::Pow(10.f, FMath::FloorToFloat(FMath::LogX(10.f, FMath::Max(ApproxDaysPerLabel, 1.f))));
		const float N = ApproxDaysPerLabel / Pow10;
		const float M = (N <= 1.f) ? 1.f : (N <= 2.f) ? 2.f : (N <= 5.f) ? 5.f : 10.f;
		return FMath::Max(1.f, M * Pow10);
	}

	const float EdgeGrabPx = 6.f;
}

void SEarth4DTimeline::Construct(const FArguments& InArgs)
{
	SubsystemGetter = InArgs._SubsystemGetter;
	OnTaskSelected = InArgs._OnTaskSelected;
}

SEarth4DTimeline::FMetrics SEarth4DTimeline::ComputeMetrics(const FVector2D& LocalSize, UEarth4DSubsystem* Sub) const
{
	FMetrics M;
	M.TrackLeft = M.GutterWidth;
	M.TrackWidth = FMath::Max(32.f, (float)LocalSize.X - M.GutterWidth - 8.f);

	float SchedMin = 0.f, SchedMax = 1.f;
	if (Sub && Sub->Schedule) { Sub->Schedule->GetBounds(SchedMin, SchedMax); }
	const float Span = FMath::Max(1.f, SchedMax - SchedMin);

	if (ZoomDaysPerView <= 0.f)
	{
		// Auto-fit the whole schedule with a little padding on each side.
		const float Pad = Span * 0.04f;
		M.MinDay = SchedMin - Pad;
		M.MaxDay = SchedMax + Pad;
	}
	else
	{
		M.MinDay = ViewMinDay;
		M.MaxDay = ViewMinDay + ZoomDaysPerView;
	}
	M.PixelsPerDay = M.TrackWidth / FMath::Max(KINDA_SMALL_NUMBER, (M.MaxDay - M.MinDay));
	return M;
}

int32 SEarth4DTimeline::RowIndexAt(const FVector2D& Local, const FMetrics& M) const
{
	if (Local.Y < M.RulerHeight) return INDEX_NONE;
	const int32 Row = FMath::FloorToInt((Local.Y - M.RulerHeight) / M.RowHeight);
	return Row >= 0 ? Row : INDEX_NONE;
}

bool SEarth4DTimeline::HitTestBar(const FVector2D& Local, const FMetrics& M, FString& OutTaskId, EDragMode& OutMode) const
{
	UEarth4DSubsystem* Sub = ResolveSub();
	if (!Sub || !Sub->Schedule) return false;
	const int32 Row = RowIndexAt(Local, M);
	if (Row == INDEX_NONE || Row >= Sub->Schedule->Tasks.Num()) return false;

	const FEarth4DTask& T = Sub->Schedule->Tasks[Row];
	const float X0 = M.DayToX(T.Start);
	const float X1 = M.DayToX(T.End);
	if (Local.X < X0 - EdgeGrabPx || Local.X > X1 + EdgeGrabPx) return false;

	OutTaskId = T.Id;
	if (FMath::Abs(Local.X - X0) <= EdgeGrabPx)      OutMode = EDragMode::ResizeStart;
	else if (FMath::Abs(Local.X - X1) <= EdgeGrabPx) OutMode = EDragMode::ResizeEnd;
	else                                             OutMode = EDragMode::MoveBar;
	return true;
}

int32 SEarth4DTimeline::OnPaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& Cull,
	FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bEnabled) const
{
	const FVector2D Size = Geo.GetLocalSize();
	UEarth4DSubsystem* Sub = ResolveSub();
	const FMetrics M = ComputeMetrics(Size, Sub);
	const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Regular", 8);
	const FSlateFontInfo FontB = FCoreStyle::GetDefaultFontStyle("Bold", 8);

	auto Rect = [&](const FVector2D& Pos, const FVector2D& Sz, const FLinearColor& Col, int32 L)
	{
		FSlateDrawElement::MakeBox(Out, L, Geo.ToPaintGeometry(Sz, FSlateLayoutTransform(Pos)), &GWhiteBrush, ESlateDrawEffect::None, Col);
	};
	auto VLine = [&](float X, float Y0, float Y1, const FLinearColor& Col, int32 L, float W = 1.f)
	{
		Rect(FVector2D(X, Y0), FVector2D(W, Y1 - Y0), Col, L);
	};
	auto Label = [&](const FString& S, const FVector2D& Pos, const FLinearColor& Col, int32 L, const FSlateFontInfo& F)
	{
		FSlateDrawElement::MakeText(Out, L, Geo.ToPaintGeometry(FVector2D(400, 16), FSlateLayoutTransform(Pos)), S, F, ESlateDrawEffect::None, Col);
	};

	// Panel + ruler backgrounds.
	Rect(FVector2D::ZeroVector, Size, C_PanelBg, Layer);
	Rect(FVector2D::ZeroVector, FVector2D(Size.X, M.RulerHeight), C_RulerBg, Layer + 1);
	Rect(FVector2D(0, 0), FVector2D(M.GutterWidth, Size.Y), FLinearColor(1, 1, 1, 0.02f), Layer + 1);

	const int32 GridLayer = Layer + 2;
	const int32 RowLayer = Layer + 3;
	const int32 BarLayer = Layer + 5;
	const int32 TextLayer = Layer + 7;
	const int32 PlayLayer = Layer + 9;

	// Day grid + ruler labels.
	const float Step = NiceStep(48.f / FMath::Max(M.PixelsPerDay, KINDA_SMALL_NUMBER));
	const float FirstTick = FMath::CeilToFloat(M.MinDay / Step) * Step;
	for (float D = FirstTick; D <= M.MaxDay; D += Step)
	{
		const float X = M.DayToX(D);
		if (X < M.TrackLeft - 1.f) continue;
		VLine(X, M.RulerHeight, Size.Y, C_Grid, GridLayer);
		VLine(X, M.RulerHeight - 5.f, M.RulerHeight, C_GridStrong, GridLayer);
		Label(FString::Printf(TEXT("%g"), D), FVector2D(X + 3.f, 5.f), C_TextDim, TextLayer, Font);
	}
	Label(LOCTEXT("DayAxis", "DAY").ToString(), FVector2D(8.f, 5.f), C_TextDim, TextLayer, FontB);

	// Task rows.
	if (Sub && Sub->Schedule)
	{
		const float Day = Sub->CurrentDay;
		const TArray<FEarth4DTask>& Tasks = Sub->Schedule->Tasks;
		for (int32 i = 0; i < Tasks.Num(); ++i)
		{
			const FEarth4DTask& T = Tasks[i];
			const float RowY = M.RulerHeight + i * M.RowHeight;
			if (i % 2 == 1) Rect(FVector2D(0, RowY), FVector2D(Size.X, M.RowHeight), C_RowAlt, RowLayer);

			// Bar colour: stage colour if assigned, else task type colour.
			FLinearColor BarCol = TypeColor(T.Type);
			if (!T.StageId.IsEmpty())
				if (const FEarth4DStage* S = Sub->Schedule->FindStage(T.StageId)) BarCol = S->Color;

			const bool bActive = (Day >= T.Start && Day < T.End);
			const bool bSelected = (T.Id == SelectedTaskId);

			// Gutter: colour swatch + name.
			Rect(FVector2D(6.f, RowY + M.RowPad), FVector2D(4.f, M.RowHeight - 2 * M.RowPad), BarCol, BarLayer);
			Label(T.Name, FVector2D(16.f, RowY + 5.f), bSelected ? C_Select : C_Text, TextLayer, bSelected ? FontB : Font);

			// Bar.
			const float X0 = FMath::Max(M.TrackLeft, M.DayToX(T.Start));
			const float X1 = FMath::Min(Size.X, M.DayToX(T.End));
			if (X1 > X0)
			{
				const FVector2D BarPos(X0, RowY + M.RowPad);
				const FVector2D BarSz(X1 - X0, M.RowHeight - 2 * M.RowPad);
				Rect(BarPos, BarSz, BarCol.CopyWithNewOpacity(bActive ? 0.95f : 0.72f), BarLayer);
				// Progress fill while animating.
				if (bActive)
				{
					const float P = FMath::Clamp((Day - T.Start) / FMath::Max(1.f, T.End - T.Start), 0.f, 1.f);
					Rect(BarPos, FVector2D(BarSz.X * P, BarSz.Y), FLinearColor(1, 1, 1, 0.22f), BarLayer + 1);
				}
				// Selection / active outline (4 thin edges).
				if (bSelected || bActive)
				{
					const FLinearColor OC = bSelected ? C_Select : C_Playhead.CopyWithNewOpacity(0.8f);
					Rect(BarPos, FVector2D(BarSz.X, 1.f), OC, BarLayer + 2);
					Rect(FVector2D(BarPos.X, BarPos.Y + BarSz.Y - 1.f), FVector2D(BarSz.X, 1.f), OC, BarLayer + 2);
					Rect(BarPos, FVector2D(1.f, BarSz.Y), OC, BarLayer + 2);
					Rect(FVector2D(BarPos.X + BarSz.X - 1.f, BarPos.Y), FVector2D(1.f, BarSz.Y), OC, BarLayer + 2);
				}
				// Element count badge.
				if (BarSz.X > 34.f)
					Label(FString::Printf(TEXT("%d"), T.ObjectIds.Num()), FVector2D(X0 + 4.f, RowY + 5.f), FLinearColor(1, 1, 1, 0.85f), TextLayer, Font);
			}
		}

		// Playhead (current day).
		const float PX = M.DayToX(Day);
		if (PX >= M.TrackLeft - 1.f && PX <= Size.X)
		{
			VLine(PX, 0.f, Size.Y, C_Playhead, PlayLayer, 1.5f);
			Rect(FVector2D(PX - 16.f, 0.f), FVector2D(34.f, M.RulerHeight - 6.f), C_Playhead.CopyWithNewOpacity(0.18f), PlayLayer);
			Label(FString::Printf(TEXT("%.1f"), Day), FVector2D(PX - 13.f, 4.f), C_Playhead, PlayLayer + 1, FontB);
		}
	}
	else
	{
		Label(LOCTEXT("NoSchedule", "Open a level with an Earth4D Site to begin.").ToString(),
			FVector2D(M.GutterWidth + 12.f, M.RulerHeight + 12.f), C_TextDim, TextLayer, Font);
	}

	return FMath::Max(Layer, PlayLayer + 2);
}

FReply SEarth4DTimeline::OnMouseButtonDown(const FGeometry& Geo, const FPointerEvent& Mouse)
{
	if (Mouse.GetEffectingButton() != EKeys::LeftMouseButton) return FReply::Unhandled();
	UEarth4DSubsystem* Sub = ResolveSub();
	if (!Sub) return FReply::Unhandled();

	const FVector2D Local = Geo.AbsoluteToLocal(Mouse.GetScreenSpacePosition());
	const FMetrics M = ComputeMetrics(Geo.GetLocalSize(), Sub);

	// Ruler strip → scrub the playhead.
	if (Local.Y < M.RulerHeight && Local.X >= M.TrackLeft)
	{
		DragMode = EDragMode::Scrub;
		Sub->SetCurrentDay(FMath::Max(0.f, M.XToDay(Local.X)));
		return FReply::Handled().CaptureMouse(AsShared());
	}

	FString TaskId; EDragMode Mode;
	if (HitTestBar(Local, M, TaskId, Mode))
	{
		SelectedTaskId = TaskId;
		OnTaskSelected.ExecuteIfBound(TaskId);
		DragMode = Mode;
		DragTaskId = TaskId;
		if (const FEarth4DTask* T = Sub->Schedule->FindTask(TaskId))
			DragGrabDayOffset = M.XToDay(Local.X) - T->Start;
		return FReply::Handled().CaptureMouse(AsShared());
	}

	// Clicked empty space → clear selection.
	SelectedTaskId.Empty();
	OnTaskSelected.ExecuteIfBound(FString());
	return FReply::Handled();
}

FReply SEarth4DTimeline::OnMouseMove(const FGeometry& Geo, const FPointerEvent& Mouse)
{
	if (DragMode == EDragMode::None || !HasMouseCapture()) return FReply::Unhandled();
	UEarth4DSubsystem* Sub = ResolveSub();
	if (!Sub || !Sub->Schedule) return FReply::Unhandled();

	const FVector2D Local = Geo.AbsoluteToLocal(Mouse.GetScreenSpacePosition());
	const FMetrics M = ComputeMetrics(Geo.GetLocalSize(), Sub);
	const float Day = M.XToDay(Local.X);

	if (DragMode == EDragMode::Scrub)
	{
		Sub->SetCurrentDay(FMath::Max(0.f, Day));
		return FReply::Handled();
	}

	FEarth4DTask* T = Sub->Schedule->FindTask(DragTaskId);
	if (!T) return FReply::Handled();

	// Snap to whole days unless Shift is held (fine drag).
	auto Snap = [&](float D) { return Mouse.IsShiftDown() ? D : FMath::RoundToFloat(D); };

	switch (DragMode)
	{
	case EDragMode::MoveBar:
	{
		const float Span = T->End - T->Start;
		const float NewStart = FMath::Max(0.f, Snap(Day - DragGrabDayOffset));
		Sub->SetTaskWindow(DragTaskId, NewStart, NewStart + Span);
		break;
	}
	case EDragMode::ResizeStart:
	{
		const float NewStart = FMath::Clamp(Snap(Day), 0.f, T->End - 1.f);
		Sub->SetTaskWindow(DragTaskId, NewStart, T->End);
		break;
	}
	case EDragMode::ResizeEnd:
	{
		const float NewEnd = FMath::Max(T->Start + 1.f, Snap(Day));
		Sub->SetTaskWindow(DragTaskId, T->Start, NewEnd);
		break;
	}
	default: break;
	}
	return FReply::Handled();
}

FReply SEarth4DTimeline::OnMouseButtonUp(const FGeometry& Geo, const FPointerEvent& Mouse)
{
	if (DragMode != EDragMode::None)
	{
		DragMode = EDragMode::None;
		DragTaskId.Empty();
		return FReply::Handled().ReleaseMouseCapture();
	}
	return FReply::Unhandled();
}

FReply SEarth4DTimeline::OnMouseWheel(const FGeometry& Geo, const FPointerEvent& Mouse)
{
	UEarth4DSubsystem* Sub = ResolveSub();
	const FMetrics M = ComputeMetrics(Geo.GetLocalSize(), Sub);

	// Zoom about the cursor's day.
	const FVector2D Local = Geo.AbsoluteToLocal(Mouse.GetScreenSpacePosition());
	const float PivotDay = M.XToDay(FMath::Max(Local.X, M.TrackLeft));
	const float CurView = (ZoomDaysPerView > 0.f) ? ZoomDaysPerView : (M.MaxDay - M.MinDay);
	const float Factor = Mouse.GetWheelDelta() > 0 ? 0.85f : 1.f / 0.85f;
	const float NewView = FMath::Clamp(CurView * Factor, 2.f, 100000.f);
	// Keep PivotDay under the cursor.
	const float PivotFrac = (PivotDay - M.MinDay) / FMath::Max(KINDA_SMALL_NUMBER, (M.MaxDay - M.MinDay));
	ZoomDaysPerView = NewView;
	ViewMinDay = PivotDay - PivotFrac * NewView;
	return FReply::Handled();
}

FCursorReply SEarth4DTimeline::OnCursorQuery(const FGeometry& Geo, const FPointerEvent& Mouse) const
{
	UEarth4DSubsystem* Sub = ResolveSub();
	if (!Sub) return FCursorReply::Unhandled();
	const FVector2D Local = Geo.AbsoluteToLocal(Mouse.GetScreenSpacePosition());
	const FMetrics M = ComputeMetrics(Geo.GetLocalSize(), Sub);
	FString TaskId; EDragMode Mode;
	if (HitTestBar(Local, M, TaskId, Mode))
	{
		if (Mode == EDragMode::ResizeStart || Mode == EDragMode::ResizeEnd)
			return FCursorReply::Cursor(EMouseCursor::ResizeLeftRight);
		return FCursorReply::Cursor(EMouseCursor::CardinalCross);
	}
	if (Local.Y < M.RulerHeight) return FCursorReply::Cursor(EMouseCursor::ResizeLeftRight);
	return FCursorReply::Unhandled();
}

#undef LOCTEXT_NAMESPACE
