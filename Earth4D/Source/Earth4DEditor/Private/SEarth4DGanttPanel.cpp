// Copyright Earth4D. Licensed for project use.
#include "SEarth4DGanttPanel.h"
#include "Earth4DSubsystem.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"
#include "Editor.h"
#include "Engine/World.h"

#define LOCTEXT_NAMESPACE "Earth4D"

void SEarth4DGanttPanel::Construct(const FArguments&)
{
	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(8)
		[
			SNew(STextBlock).Text(LOCTEXT("Title", "Earth4D — 4D Construction Sequencing"))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(8, 0)
		[
			SNew(SButton)
			.Text(LOCTEXT("AddTask", "+ Add construction task"))
			.OnClicked(this, &SEarth4DGanttPanel::OnAddTaskClicked)
		]
		+ SVerticalBox::Slot().FillHeight(1).Padding(8)
		[
			SNew(SBorder)
			[
				SNew(STextBlock)
				.Text(this, &SEarth4DGanttPanel::GetSummaryText)
				.AutoWrapText(true)
			]
		]
		// TODO (phase 4): draggable Gantt bars, stage lanes, element tree + editor,
		// and the chat box wired to UEarth4DChatClient. All actions route through
		// UEarth4DSubsystem so they match the chat/MCP exactly.
	];
}

UEarth4DSubsystem* SEarth4DGanttPanel::ResolveSubsystem() const
{
	if (GEditor)
		if (UWorld* W = GEditor->GetEditorWorldContext().World())
			return W->GetSubsystem<UEarth4DSubsystem>();
	return nullptr;
}

FReply SEarth4DGanttPanel::OnAddTaskClicked()
{
	if (UEarth4DSubsystem* Sub = ResolveSubsystem())
	{
		FString Id;
		Sub->AddTask(TEXT("New Task"), EEarth4DTaskType::Construction, 0.f, 10.f, Id);
	}
	return FReply::Handled();
}

FText SEarth4DGanttPanel::GetSummaryText() const
{
	if (UEarth4DSubsystem* Sub = const_cast<SEarth4DGanttPanel*>(this)->ResolveSubsystem())
		return FText::FromString(Sub->GetScheduleSummary());
	return LOCTEXT("NoWorld", "Open a level with the Earth4D template to begin.");
}

#undef LOCTEXT_NAMESPACE
