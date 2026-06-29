// Copyright Earth4D. Licensed for project use.
// Models tree + element editor (web app's model tree / selection sets + per-element
// overrides). Lists the registered elements, drives UEarth4DSubsystem selection,
// and edits the FEarth4DObjectEdit of every selected element. Also exposes
// "assign selection to task" so the tree and the Gantt work together.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

class UEarth4DSubsystem;

class SEarth4DElementsPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SEarth4DElementsPanel) {}
		SLATE_ARGUMENT(TFunction<UEarth4DSubsystem*()>, SubsystemGetter)
		/** The Gantt's currently-selected task id, so "assign to task" knows the target. */
		SLATE_ATTRIBUTE(FString, ActiveTaskId)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Re-read the element registry from the schedule (called on OnScheduleChanged). */
	void RefreshList();

private:
	UEarth4DSubsystem* ResolveSub() const { return SubsystemGetter ? SubsystemGetter() : nullptr; }

	TSharedRef<ITableRow> OnGenerateRow(TSharedPtr<FString> Item, const TSharedRef<STableViewBase>& Owner);
	void OnSelectionChanged(TSharedPtr<FString> Item, ESelectInfo::Type SelectInfo);

	// Element-editor accessors operate on the subsystem's selection (multi-edit).
	struct FEarth4DObjectEdit GetEditTemplate() const; // first selected element's edit (or defaults)
	void ApplyEditField(TFunctionRef<void(struct FEarth4DObjectEdit&)> Mutator);

	FText GetSelectionSummary() const;
	EVisibility GetEditorVisibility() const;

	FReply OnAssignToTask();
	FReply OnFilterCommitted();

	// Selection sets (named reusable selections).
	FReply OnSaveSelectionSet();
	TSharedRef<SWidget> BuildSelectionSetsMenu();

	/** Enum dropdown bound to a getter/setter (Style / Direction overrides). */
	TSharedRef<SWidget> MakeEnumCombo(class UEnum* Enum, TFunction<int32()> Get, TFunction<void(int32)> Set);
	/** Keepalive storage for the enum combo option lists. */
	TArray<TSharedPtr<TArray<TSharedPtr<FString>>>> ComboSources;

	TFunction<UEarth4DSubsystem*()> SubsystemGetter;
	TAttribute<FString> ActiveTaskId;

	TArray<TSharedPtr<FString>> ElementIds;         // element ids backing the list
	TSharedPtr<SListView<TSharedPtr<FString>>> ListView;
	FString Filter;
	bool bSyncingSelection = false;                 // guard re-entrancy when we set list selection
};
