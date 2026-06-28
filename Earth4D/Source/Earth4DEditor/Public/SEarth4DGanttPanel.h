// Copyright Earth4D. Licensed for project use.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

/**
 * The 4D authoring panel (Slate): task/stage Gantt with draggable bars, an element
 * tree, the element editor, and the chat box. STUB — lays out the structure and
 * binds to UEarth4DSubsystem; the full interactive Gantt is a later pass (see
 * ARCHITECTURE.md §7, phase 4).
 */
class SEarth4DGanttPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SEarth4DGanttPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	class UEarth4DSubsystem* ResolveSubsystem() const;
	FReply OnAddTaskClicked();
	FText GetSummaryText() const;
};
