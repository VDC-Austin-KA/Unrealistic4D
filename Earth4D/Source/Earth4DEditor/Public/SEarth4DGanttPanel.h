// Copyright Earth4D. Licensed for project use.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class UEarth4DSubsystem;
class SEarth4DTimeline;
class SEarth4DElementsPanel;
class SEarth4DChatPanel;

/**
 * The Earth4D authoring panel (Slate): playback transport, the draggable 4D Gantt
 * timeline, a task inspector, the models tree + element editor, and the in-app chat.
 * Every action routes through UEarth4DSubsystem so the UI, chat, and MCP stay in
 * lockstep (ARCHITECTURE.md §4). Polls the subsystem Revision to refresh.
 */
class SEarth4DGanttPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SEarth4DGanttPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual void Tick(const FGeometry& AllottedGeometry, const double CurrentTime, const float DeltaTime) override;

private:
	UEarth4DSubsystem* ResolveSubsystem() const;

	// Transport.
	FReply OnPlayPause();
	FText GetPlayPauseLabel() const;
	float GetCurrentDay() const;
	void OnCurrentDayChanged(float Day);
	float GetSpeed() const;
	void OnSpeedChanged(float Speed);

	// Task / stage creation.
	FReply OnAddTaskClicked();
	FReply OnAddStageClicked();
	FReply OnDeleteTaskClicked();

	// Selected-task inspector.
	void OnTaskSelected(FString TaskId);
	const struct FEarth4DTask* GetSelectedTask() const;
	EVisibility GetInspectorVisibility() const;
	FText GetSelectedTaskName() const;
	void OnRenameCommitted(const FText& Text, ETextCommit::Type CommitType);

	// Enum combo helpers (Type / Style / Direction).
	TSharedRef<SWidget> BuildEnumCombo(class UEnum* Enum, TFunction<int32()> Get, TFunction<void(int32)> Set);

	FText GetSummaryText() const;

	TFunction<UEarth4DSubsystem*()> Getter;
	TSharedPtr<SEarth4DTimeline> Timeline;
	TSharedPtr<SEarth4DElementsPanel> ElementsPanel;
	TSharedPtr<SEarth4DChatPanel> ChatPanel;

	FString SelectedTaskId;
	int32 LastSeenRevision = -1;

	// Keepalive storage for combo option lists (one per enum used).
	TArray<TSharedPtr<TArray<TSharedPtr<FString>>>> ComboSources;
};
