// Copyright Earth4D. Licensed for project use.
#include "SEarth4DGanttPanel.h"
#include "SEarth4DTimeline.h"
#include "SEarth4DElementsPanel.h"
#include "SEarth4DChatPanel.h"
#include "Earth4DSubsystem.h"
#include "Earth4DSchedule.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Layout/SSeparator.h"
#include "Editor.h"
#include "Engine/World.h"

#define LOCTEXT_NAMESPACE "Earth4D"

void SEarth4DGanttPanel::Construct(const FArguments&)
{
	Getter = [this]() { return ResolveSubsystem(); };

	// Task inspector grid (populated; visibility toggled by selection).
	int32 Row = 0;
	TSharedRef<SGridPanel> Insp = SNew(SGridPanel).FillColumn(1, 1.f);
	auto AddRow = [&](const FText& Lbl, TSharedRef<SWidget> W)
	{
		Insp->AddSlot(0, Row).Padding(4, 3)[ SNew(STextBlock).Text(Lbl).ColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.65f, 0.72f))) ];
		Insp->AddSlot(1, Row).Padding(4, 3)[ W ];
		++Row;
	};

	AddRow(LOCTEXT("Type", "Type"), BuildEnumCombo(StaticEnum<EEarth4DTaskType>(),
		[this]{ const FEarth4DTask* T = GetSelectedTask(); return T ? (int32)T->Type : 0; },
		[this](int32 V){ if (UEarth4DSubsystem* S = ResolveSubsystem()) S->SetTaskType(SelectedTaskId, (EEarth4DTaskType)V); }));
	AddRow(LOCTEXT("Style", "Style"), BuildEnumCombo(StaticEnum<EEarth4DAnimStyle>(),
		[this]{ const FEarth4DTask* T = GetSelectedTask(); return T ? (int32)T->Style : 0; },
		[this](int32 V){ if (UEarth4DSubsystem* S = ResolveSubsystem()) S->SetTaskStyle(SelectedTaskId, (EEarth4DAnimStyle)V); }));
	AddRow(LOCTEXT("Dir", "Direction"), BuildEnumCombo(StaticEnum<EEarth4DDirection>(),
		[this]{ const FEarth4DTask* T = GetSelectedTask(); return T ? (int32)T->Direction : 0; },
		[this](int32 V){ if (UEarth4DSubsystem* S = ResolveSubsystem()) S->SetTaskDirection(SelectedTaskId, (EEarth4DDirection)V); }));

	AddRow(LOCTEXT("Dur", "Duration (days)"),
		SNew(SSpinBox<float>).MinValue(1).MaxValue(100000).Delta(1)
		.Value_Lambda([this]{ const FEarth4DTask* T = GetSelectedTask(); return T ? (T->End - T->Start) : 1.f; })
		.OnValueChanged_Lambda([this](float V){ if (UEarth4DSubsystem* S = ResolveSubsystem()) S->SetTaskDuration(SelectedTaskId, V); }));
	AddRow(LOCTEXT("Start", "Start day"),
		SNew(SSpinBox<float>).MinValue(0).MaxValue(100000).Delta(1)
		.Value_Lambda([this]{ const FEarth4DTask* T = GetSelectedTask(); return T ? T->Start : 0.f; })
		.OnValueChanged_Lambda([this](float V){ const FEarth4DTask* T = GetSelectedTask(); if (UEarth4DSubsystem* S = ResolveSubsystem()) if (T) S->SetTaskWindow(SelectedTaskId, V, V + (T->End - T->Start)); }));
	AddRow(LOCTEXT("Stagger", "Stagger (days)"),
		SNew(SSpinBox<float>).MinValue(0).MaxValue(1000).Delta(0.1f)
		.Value_Lambda([this]{ const FEarth4DTask* T = GetSelectedTask(); return T ? T->Stagger : 0.f; })
		.OnValueChanged_Lambda([this](float V){ if (UEarth4DSubsystem* S = ResolveSubsystem()) S->SetTaskStagger(SelectedTaskId, V); }));
	AddRow(LOCTEXT("Distance", "Distance (m)"),
		SNew(SSpinBox<float>).MinValue(0).MaxValue(100000).Delta(1)
		.Value_Lambda([this]{ const FEarth4DTask* T = GetSelectedTask(); return T ? T->Distance : 0.f; })
		.OnValueChanged_Lambda([this](float V){ if (UEarth4DSubsystem* S = ResolveSubsystem()) if (FEarth4DTask* T = S->Schedule ? S->Schedule->FindTask(SelectedTaskId) : nullptr) { T->Distance = V; S->EvaluateAndApply(S->CurrentDay); } }));
	AddRow(LOCTEXT("Overlap", "Overlap (0..1)"),
		SNew(SSpinBox<float>).MinValue(0).MaxValue(1).Delta(0.05f)
		.Value_Lambda([this]{ const FEarth4DTask* T = GetSelectedTask(); return T ? T->Overlap : 0.f; })
		.OnValueChanged_Lambda([this](float V){ if (UEarth4DSubsystem* S = ResolveSubsystem()) if (FEarth4DTask* T = S->Schedule ? S->Schedule->FindTask(SelectedTaskId) : nullptr) { T->Overlap = V; S->EvaluateAndApply(S->CurrentDay); } }));

	ChildSlot
	[
		SNew(SVerticalBox)

		// ---- Transport bar ----
		+ SVerticalBox::Slot().AutoHeight().Padding(8, 8, 8, 4)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)
			[ SNew(SButton).Text(LOCTEXT("AddTask", "+ Task")).OnClicked(this, &SEarth4DGanttPanel::OnAddTaskClicked) ]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)
			[ SNew(SButton).Text(LOCTEXT("AddStage", "+ Stage")).OnClicked(this, &SEarth4DGanttPanel::OnAddStageClicked) ]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)
			[ SNew(SButton).Text(LOCTEXT("Save", "Save")).ToolTipText(LOCTEXT("SaveTip", "Save the project (Saved/Earth4D)."))
				.OnClicked_Lambda([this]{ if (UEarth4DSubsystem* S = ResolveSubsystem()) S->SaveProject(FString()); return FReply::Handled(); }) ]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 12, 0)
			[ SNew(SButton).Text(LOCTEXT("Load", "Load"))
				.OnClicked_Lambda([this]{ if (UEarth4DSubsystem* S = ResolveSubsystem()) S->LoadProject(FString()); return FReply::Handled(); }) ]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
			[ SNew(SButton).Text(this, &SEarth4DGanttPanel::GetPlayPauseLabel).OnClicked(this, &SEarth4DGanttPanel::OnPlayPause) ]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 4, 0)
			[ SNew(STextBlock).Text(LOCTEXT("DayLbl", "Day")) ]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
			[ SNew(SBox).WidthOverride(72)
				[ SNew(SSpinBox<float>).MinValue(0).MaxValue(100000).Delta(0.5f)
					.Value(this, &SEarth4DGanttPanel::GetCurrentDay)
					.OnValueChanged(this, &SEarth4DGanttPanel::OnCurrentDayChanged) ] ]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 4, 0)
			[ SNew(STextBlock).Text(LOCTEXT("SpeedLbl", "Days/s")) ]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 12, 0)
			[ SNew(SBox).WidthOverride(64)
				[ SNew(SSpinBox<float>).MinValue(0.05f).MaxValue(100).Delta(0.1f)
					.Value(this, &SEarth4DGanttPanel::GetSpeed)
					.OnValueChanged(this, &SEarth4DGanttPanel::OnSpeedChanged) ] ]
			+ SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
			[ SNew(STextBlock).Text(this, &SEarth4DGanttPanel::GetSummaryText).ColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.55f, 0.62f))) ]
		]

		// ---- Timeline (the spine) ----
		+ SVerticalBox::Slot().FillHeight(1.1f).Padding(8, 0)
		[
			SNew(SBorder).Padding(0)
			[
				SAssignNew(Timeline, SEarth4DTimeline)
				.SubsystemGetter(Getter)
				.OnTaskSelected(FOnEarth4DTaskSelected::CreateSP(this, &SEarth4DGanttPanel::OnTaskSelected))
			]
		]

		// ---- Bottom: inspector | models | chat ----
		+ SVerticalBox::Slot().FillHeight(1.0f).Padding(8, 6, 8, 8)
		[
			SNew(SSplitter)
			+ SSplitter::Slot().Value(0.28f)
			[
				SNew(SBorder).Padding(4)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(2, 2, 2, 4)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1)
						[ SNew(SEditableTextBox)
							.Text(this, &SEarth4DGanttPanel::GetSelectedTaskName)
							.OnTextCommitted(this, &SEarth4DGanttPanel::OnRenameCommitted)
							.HintText(LOCTEXT("TaskName", "Task name")) ]
						+ SHorizontalBox::Slot().AutoWidth().Padding(4, 0, 0, 0)
						[ SNew(SButton).Text(LOCTEXT("Del", "Delete")).OnClicked(this, &SEarth4DGanttPanel::OnDeleteTaskClicked) ]
					]
					+ SVerticalBox::Slot().AutoHeight()
					[ SNew(SBox).Visibility(this, &SEarth4DGanttPanel::GetInspectorVisibility)[ Insp ] ]
					+ SVerticalBox::Slot().FillHeight(1)
					[ SNew(STextBlock).Visibility_Lambda([this]{ return GetInspectorVisibility() == EVisibility::Visible ? EVisibility::Collapsed : EVisibility::Visible; })
						.AutoWrapText(true).ColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.55f, 0.62f)))
						.Text(LOCTEXT("PickTask", "Select a task bar in the timeline to edit it, or add one above.")) ]
				]
			]
			+ SSplitter::Slot().Value(0.34f)
			[
				SNew(SBorder).Padding(0)
				[
					SAssignNew(ElementsPanel, SEarth4DElementsPanel)
					.SubsystemGetter(Getter)
					.ActiveTaskId_Lambda([this]{ return SelectedTaskId; })
				]
			]
			+ SSplitter::Slot().Value(0.38f)
			[
				SNew(SBorder).Padding(0)
				[ SAssignNew(ChatPanel, SEarth4DChatPanel).SubsystemGetter(Getter) ]
			]
		]
	];
}

void SEarth4DGanttPanel::Tick(const FGeometry& Geo, const double Time, const float Delta)
{
	SCompoundWidget::Tick(Geo, Time, Delta);
	// Cheap refresh: rebuild the element list only when the schedule actually changed.
	if (UEarth4DSubsystem* Sub = ResolveSubsystem())
	{
		if (Sub->Revision != LastSeenRevision)
		{
			LastSeenRevision = Sub->Revision;
			if (ElementsPanel.IsValid()) ElementsPanel->RefreshList();
			// Drop a stale task selection.
			if (!SelectedTaskId.IsEmpty() && Sub->Schedule && !Sub->Schedule->FindTask(SelectedTaskId))
				SelectedTaskId.Empty();
		}
	}
}

UEarth4DSubsystem* SEarth4DGanttPanel::ResolveSubsystem() const
{
	if (GEditor)
	{
		if (GEditor->PlayWorld) return GEditor->PlayWorld->GetSubsystem<UEarth4DSubsystem>();
		if (UWorld* W = GEditor->GetEditorWorldContext().World()) return W->GetSubsystem<UEarth4DSubsystem>();
	}
	return nullptr;
}

// ---- Transport ----
FReply SEarth4DGanttPanel::OnPlayPause()
{
	if (UEarth4DSubsystem* S = ResolveSubsystem()) { if (S->bPlaying) S->Pause(); else S->Play(); }
	return FReply::Handled();
}
FText SEarth4DGanttPanel::GetPlayPauseLabel() const
{
	UEarth4DSubsystem* S = ResolveSubsystem();
	return (S && S->bPlaying) ? LOCTEXT("Pause", "❚❚ Pause") : LOCTEXT("Play", "▶ Play");
}
float SEarth4DGanttPanel::GetCurrentDay() const { UEarth4DSubsystem* S = ResolveSubsystem(); return S ? S->CurrentDay : 0.f; }
void SEarth4DGanttPanel::OnCurrentDayChanged(float Day) { if (UEarth4DSubsystem* S = ResolveSubsystem()) S->SetCurrentDay(Day); }
float SEarth4DGanttPanel::GetSpeed() const { UEarth4DSubsystem* S = ResolveSubsystem(); return S ? S->DaysPerSecond : 1.f; }
void SEarth4DGanttPanel::OnSpeedChanged(float Speed) { if (UEarth4DSubsystem* S = ResolveSubsystem()) S->SetSpeed(Speed); }

// ---- Creation ----
FReply SEarth4DGanttPanel::OnAddTaskClicked()
{
	if (UEarth4DSubsystem* S = ResolveSubsystem())
	{
		FString Id;
		float Start = 0.f;
		if (S->Schedule && S->Schedule->Tasks.Num() > 0) { float Mn, Mx; S->Schedule->GetBounds(Mn, Mx); Start = Mx; }
		S->AddTask(TEXT("New Task"), EEarth4DTaskType::Construction, Start, 10.f, Id);
		SelectedTaskId = Id;
		if (Timeline.IsValid()) Timeline->SetSelectedTaskId(Id);
	}
	return FReply::Handled();
}
FReply SEarth4DGanttPanel::OnAddStageClicked()
{
	if (UEarth4DSubsystem* S = ResolveSubsystem()) { FString Id; S->AddStage(TEXT("New Stage"), Id); }
	return FReply::Handled();
}
FReply SEarth4DGanttPanel::OnDeleteTaskClicked()
{
	if (UEarth4DSubsystem* S = ResolveSubsystem()) S->RemoveTask(SelectedTaskId);
	SelectedTaskId.Empty();
	if (Timeline.IsValid()) Timeline->SetSelectedTaskId(FString());
	return FReply::Handled();
}

// ---- Selected-task inspector ----
void SEarth4DGanttPanel::OnTaskSelected(FString TaskId) { SelectedTaskId = TaskId; }
const FEarth4DTask* SEarth4DGanttPanel::GetSelectedTask() const
{
	UEarth4DSubsystem* S = ResolveSubsystem();
	return (S && S->Schedule) ? S->Schedule->FindTask(SelectedTaskId) : nullptr;
}
EVisibility SEarth4DGanttPanel::GetInspectorVisibility() const { return GetSelectedTask() ? EVisibility::Visible : EVisibility::Collapsed; }
FText SEarth4DGanttPanel::GetSelectedTaskName() const { const FEarth4DTask* T = GetSelectedTask(); return T ? FText::FromString(T->Name) : FText::GetEmpty(); }
void SEarth4DGanttPanel::OnRenameCommitted(const FText& Text, ETextCommit::Type CommitType)
{
	if (CommitType == ETextCommit::OnEnter || CommitType == ETextCommit::OnUserMovedFocus)
		if (UEarth4DSubsystem* S = ResolveSubsystem())
			if (GetSelectedTask()) S->RenameTask(SelectedTaskId, Text.ToString());
}

// ---- Enum combo ----
TSharedRef<SWidget> SEarth4DGanttPanel::BuildEnumCombo(UEnum* Enum, TFunction<int32()> Get, TFunction<void(int32)> Set)
{
	TSharedPtr<TArray<TSharedPtr<FString>>> Options = MakeShared<TArray<TSharedPtr<FString>>>();
	const int32 Count = Enum->NumEnums() - 1; // drop the implicit _MAX
	for (int32 i = 0; i < Count; ++i)
		Options->Add(MakeShared<FString>(Enum->GetDisplayNameTextByIndex(i).ToString()));
	ComboSources.Add(Options); // keepalive

	TWeakPtr<TArray<TSharedPtr<FString>>> WeakOpts = Options;
	return SNew(SComboBox<TSharedPtr<FString>>)
		.OptionsSource(Options.Get())
		.OnGenerateWidget_Lambda([](TSharedPtr<FString> In){ return SNew(STextBlock).Text(FText::FromString(*In)); })
		.OnSelectionChanged_Lambda([Set, WeakOpts](TSharedPtr<FString> In, ESelectInfo::Type Info)
		{
			if (Info == ESelectInfo::Direct || !In.IsValid()) return;
			if (TSharedPtr<TArray<TSharedPtr<FString>>> Opts = WeakOpts.Pin())
				Set(Opts->IndexOfByKey(In));
		})
		[
			SNew(STextBlock).Text_Lambda([Get, WeakOpts]()
			{
				if (TSharedPtr<TArray<TSharedPtr<FString>>> Opts = WeakOpts.Pin())
				{
					const int32 V = Get();
					if (Opts->IsValidIndex(V)) return FText::FromString(*(*Opts)[V]);
				}
				return FText::GetEmpty();
			})
		];
}

FText SEarth4DGanttPanel::GetSummaryText() const
{
	UEarth4DSubsystem* S = ResolveSubsystem();
	if (!S) return LOCTEXT("NoWorld", "Open a level with the Earth4D template to begin.");
	if (!S->Schedule) return LOCTEXT("NoSched", "No schedule.");
	return FText::Format(LOCTEXT("Summary", "{0} tasks · {1} stages · {2} elements"),
		FText::AsNumber(S->Schedule->Tasks.Num()), FText::AsNumber(S->Schedule->Stages.Num()), FText::AsNumber(S->Schedule->Elements.Num()));
}

#undef LOCTEXT_NAMESPACE
