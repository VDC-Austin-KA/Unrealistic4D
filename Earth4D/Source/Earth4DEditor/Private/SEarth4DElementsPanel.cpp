// Copyright Earth4D. Licensed for project use.
#include "SEarth4DElementsPanel.h"
#include "Earth4DSubsystem.h"
#include "Earth4DSchedule.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Colors/SColorPicker.h"

#define LOCTEXT_NAMESPACE "Earth4D"

void SEarth4DElementsPanel::Construct(const FArguments& InArgs)
{
	SubsystemGetter = InArgs._SubsystemGetter;
	ActiveTaskId = InArgs._ActiveTaskId;

	auto MakeSpin = [this](TFunction<float()> Get, TFunction<void(float)> Set, float Min, float Max, float Delta)
	{
		return SNew(SSpinBox<float>)
			.MinValue(Min).MaxValue(Max).Delta(Delta)
			.Value_Lambda([Get]() { return Get(); })
			.OnValueChanged_Lambda([Set](float V) { Set(V); });
	};

	int32 GridRow = 0;
	TSharedRef<SGridPanel> Grid = SNew(SGridPanel).FillColumn(1, 1.f);
	auto AddRow = [&](const FText& Lbl, TSharedRef<SWidget> Field)
	{
		Grid->AddSlot(0, GridRow).Padding(4, 3)[ SNew(STextBlock).Text(Lbl).ColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.65f, 0.72f))) ];
		Grid->AddSlot(1, GridRow).Padding(4, 3)[ Field ];
		++GridRow;
	};

	AddRow(LOCTEXT("OffE", "Offset E (m)"), MakeSpin(
		[this]{ return (float)GetEditTemplate().Offset.X; },
		[this](float V){ ApplyEditField([V](FEarth4DObjectEdit& E){ E.Offset.X = V; }); }, -100000, 100000, 0.5f));
	AddRow(LOCTEXT("OffN", "Offset N (m)"), MakeSpin(
		[this]{ return (float)GetEditTemplate().Offset.Y; },
		[this](float V){ ApplyEditField([V](FEarth4DObjectEdit& E){ E.Offset.Y = V; }); }, -100000, 100000, 0.5f));
	AddRow(LOCTEXT("OffU", "Offset Up (m)"), MakeSpin(
		[this]{ return (float)GetEditTemplate().Offset.Z; },
		[this](float V){ ApplyEditField([V](FEarth4DObjectEdit& E){ E.Offset.Z = V; }); }, -100000, 100000, 0.5f));
	AddRow(LOCTEXT("Rot", "Rotation (deg)"), MakeSpin(
		[this]{ return GetEditTemplate().RotationDeg; },
		[this](float V){ ApplyEditField([V](FEarth4DObjectEdit& E){ E.RotationDeg = V; }); }, -360, 360, 1.f));
	AddRow(LOCTEXT("Scale", "Scale"), MakeSpin(
		[this]{ return GetEditTemplate().Scale; },
		[this](float V){ ApplyEditField([V](FEarth4DObjectEdit& E){ E.Scale = FMath::Max(0.001f, V); }); }, 0.001f, 1000, 0.05f));
	AddRow(LOCTEXT("Opacity", "Opacity"), MakeSpin(
		[this]{ return GetEditTemplate().Opacity; },
		[this](float V){ ApplyEditField([V](FEarth4DObjectEdit& E){ E.Opacity = V; }); }, 0.f, 1.f, 0.02f));
	AddRow(LOCTEXT("Appear", "Appear day (-1=off)"), MakeSpin(
		[this]{ return GetEditTemplate().AppearDay; },
		[this](float V){ ApplyEditField([V](FEarth4DObjectEdit& E){ E.AppearDay = V; }); }, -1.f, 100000, 1.f));
	AddRow(LOCTEXT("Disappear", "Disappear day (-1=off)"), MakeSpin(
		[this]{ return GetEditTemplate().DisappearDay; },
		[this](float V){ ApplyEditField([V](FEarth4DObjectEdit& E){ E.DisappearDay = V; }); }, -1.f, 100000, 1.f));

	// Hidden toggle.
	AddRow(LOCTEXT("Hidden", "Hidden"),
		SNew(SCheckBox)
		.IsChecked_Lambda([this]{ return GetEditTemplate().bHidden ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
		.OnCheckStateChanged_Lambda([this](ECheckBoxState S){ const bool b = (S == ECheckBoxState::Checked); ApplyEditField([b](FEarth4DObjectEdit& E){ E.bHidden = b; }); }));

	// Colour override: swatch opens the picker; checkbox toggles whether it's applied.
	AddRow(LOCTEXT("Colour", "Colour override"),
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SCheckBox)
			.IsChecked_Lambda([this]{ return GetEditTemplate().bHasColor ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
			.OnCheckStateChanged_Lambda([this](ECheckBoxState S){ const bool b = (S == ECheckBoxState::Checked); ApplyEditField([b](FEarth4DObjectEdit& E){ E.bHasColor = b; }); })
		]
		+ SHorizontalBox::Slot().FillWidth(1).Padding(6, 0).VAlign(VAlign_Center)
		[
			SNew(SColorBlock)
			.Color_Lambda([this]{ return GetEditTemplate().Color; })
			.Size(FVector2D(48, 16))
			.OnMouseButtonDown_Lambda([this](const FGeometry&, const FPointerEvent&) -> FReply
			{
				FColorPickerArgs Args;
				Args.bUseAlpha = false;
				Args.InitialColor = GetEditTemplate().Color;
				Args.OnColorCommitted = FOnLinearColorValueChanged::CreateLambda([this](FLinearColor C)
				{
					ApplyEditField([C](FEarth4DObjectEdit& E){ E.Color = C; E.bHasColor = true; });
				});
				OpenColorPicker(Args);
				return FReply::Handled();
			})
		]);

	ChildSlot
	[
		SNew(SVerticalBox)
		// Header + filter + assign.
		+ SVerticalBox::Slot().AutoHeight().Padding(6, 6, 6, 2)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
			[ SNew(STextBlock).Text(LOCTEXT("Models", "MODELS")).ColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.55f, 0.62f))) ]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("AssignSel", "Assign → task"))
				.ToolTipText(LOCTEXT("AssignSelTip", "Add the selected elements to the task selected in the Gantt."))
				.OnClicked(this, &SEarth4DElementsPanel::OnAssignToTask)
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(6, 2)
		[
			SNew(SSearchBox)
			.HintText(LOCTEXT("FilterHint", "Filter by name / path…"))
			.OnTextChanged_Lambda([this](const FText& T){ Filter = T.ToString(); RefreshList(); })
		]
		// Element list.
		+ SVerticalBox::Slot().FillHeight(1).Padding(6, 2)
		[
			SNew(SBorder).BorderBackgroundColor(FLinearColor(0, 0, 0, 0.25f))
			[
				SAssignNew(ListView, SListView<TSharedPtr<FString>>)
				.ListItemsSource(&ElementIds)
				.SelectionMode(ESelectionMode::Multi)
				.OnGenerateRow(this, &SEarth4DElementsPanel::OnGenerateRow)
				.OnSelectionChanged(this, &SEarth4DElementsPanel::OnSelectionChanged)
			]
		]
		// Selection summary + editor.
		+ SVerticalBox::Slot().AutoHeight().Padding(6, 4, 6, 2)
		[ SNew(STextBlock).Text(this, &SEarth4DElementsPanel::GetSelectionSummary) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(6, 2, 6, 8)
		[
			SNew(SBorder)
			.Visibility(this, &SEarth4DElementsPanel::GetEditorVisibility)
			.BorderBackgroundColor(FLinearColor(0, 0, 0, 0.2f))
			.Padding(4)
			[ Grid ]
		]
	];

	RefreshList();
}

void SEarth4DElementsPanel::RefreshList()
{
	UEarth4DSubsystem* Sub = ResolveSub();
	ElementIds.Reset();
	if (Sub && Sub->Schedule)
	{
		const FString F = Filter.TrimStartAndEnd().ToLower();
		for (const FEarth4DElement& E : Sub->Schedule->Elements)
		{
			if (F.IsEmpty() || E.DisplayName.ToLower().Contains(F) || E.Path.ToLower().Contains(F))
				ElementIds.Add(MakeShared<FString>(E.Id));
		}
	}
	if (ListView.IsValid())
	{
		ListView->RequestListRefresh();
		// Reflect the subsystem's selection back into the list view.
		if (Sub && !bSyncingSelection)
		{
			TGuardValue<bool> Guard(bSyncingSelection, true);
			ListView->ClearSelection();
			for (const TSharedPtr<FString>& Item : ElementIds)
				if (Sub->SelectedElementIds.Contains(*Item))
					ListView->SetItemSelection(Item, true);
		}
	}
}

TSharedRef<ITableRow> SEarth4DElementsPanel::OnGenerateRow(TSharedPtr<FString> Item, const TSharedRef<STableViewBase>& Owner)
{
	UEarth4DSubsystem* Sub = ResolveSub();
	FString Name = *Item;
	if (Sub && Sub->Schedule)
		if (const FEarth4DElement* E = Sub->Schedule->FindElement(*Item))
			Name = E->DisplayName.IsEmpty() ? E->Id : E->DisplayName;

	return SNew(STableRow<TSharedPtr<FString>>, Owner)
		.Padding(FMargin(4, 2))
		[
			SNew(STextBlock).Text(FText::FromString(Name))
		];
}

void SEarth4DElementsPanel::OnSelectionChanged(TSharedPtr<FString>, ESelectInfo::Type SelectInfo)
{
	if (bSyncingSelection || SelectInfo == ESelectInfo::Direct) return;
	UEarth4DSubsystem* Sub = ResolveSub();
	if (!Sub || !ListView.IsValid()) return;

	TArray<FString> Ids;
	for (const TSharedPtr<FString>& Item : ListView->GetSelectedItems()) Ids.Add(*Item);
	TGuardValue<bool> Guard(bSyncingSelection, true);
	Sub->SelectElements(Ids, /*bAdd=*/false);
}

FEarth4DObjectEdit SEarth4DElementsPanel::GetEditTemplate() const
{
	UEarth4DSubsystem* Sub = ResolveSub();
	if (Sub && Sub->Schedule)
		for (const FString& Id : Sub->SelectedElementIds)
			if (const FEarth4DElement* E = Sub->Schedule->FindElement(Id))
				return E->Edit;
	return FEarth4DObjectEdit();
}

void SEarth4DElementsPanel::ApplyEditField(TFunctionRef<void(FEarth4DObjectEdit&)> Mutator)
{
	UEarth4DSubsystem* Sub = ResolveSub();
	if (!Sub || !Sub->Schedule) return;
	for (const FString& Id : Sub->SelectedElementIds)
	{
		if (const FEarth4DElement* E = Sub->Schedule->FindElement(Id))
		{
			FEarth4DObjectEdit Edit = E->Edit;
			Mutator(Edit);
			Sub->SetElementEdit(Id, Edit);
		}
	}
}

FText SEarth4DElementsPanel::GetSelectionSummary() const
{
	UEarth4DSubsystem* Sub = ResolveSub();
	const int32 N = Sub ? Sub->SelectedElementIds.Num() : 0;
	if (N == 0) return LOCTEXT("NoSel", "No elements selected.");
	if (N == 1) return LOCTEXT("OneSel", "1 element selected — editing overrides:");
	return FText::Format(LOCTEXT("ManySel", "{0} elements selected — editing overrides:"), FText::AsNumber(N));
}

EVisibility SEarth4DElementsPanel::GetEditorVisibility() const
{
	UEarth4DSubsystem* Sub = ResolveSub();
	return (Sub && Sub->SelectedElementIds.Num() > 0) ? EVisibility::Visible : EVisibility::Collapsed;
}

FReply SEarth4DElementsPanel::OnAssignToTask()
{
	UEarth4DSubsystem* Sub = ResolveSub();
	const FString TaskId = ActiveTaskId.Get();
	if (Sub && !TaskId.IsEmpty())
		Sub->AssignElementsToTask(TaskId, Sub->SelectedElementIds, /*bAdd=*/true);
	return FReply::Handled();
}

FReply SEarth4DElementsPanel::OnFilterCommitted()
{
	RefreshList();
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
