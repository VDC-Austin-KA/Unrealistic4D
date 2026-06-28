// Copyright Earth4D. Licensed for project use.
#include "SEarth4DChatPanel.h"
#include "Earth4DSubsystem.h"
#include "Earth4DSettings.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "Earth4D"

namespace
{
	FLinearColor RoleColor(SEarth4DChatPanel::FChatLine::ERole R)
	{
		using ERole = SEarth4DChatPanel::FChatLine::ERole;
		switch (R)
		{
		case ERole::User:      return FLinearColor(0.85f, 0.88f, 0.95f);
		case ERole::Assistant: return FLinearColor(0.62f, 0.80f, 1.0f);
		case ERole::Tool:      return FLinearColor(0.55f, 0.78f, 0.55f);
		case ERole::Error:     return FLinearColor(0.95f, 0.45f, 0.40f);
		default:               return FLinearColor(0.5f, 0.55f, 0.62f);
		}
	}
	FString RolePrefix(SEarth4DChatPanel::FChatLine::ERole R)
	{
		using ERole = SEarth4DChatPanel::FChatLine::ERole;
		switch (R)
		{
		case ERole::User:      return TEXT("You");
		case ERole::Assistant: return TEXT("Claude");
		case ERole::Tool:      return TEXT("· tool");
		case ERole::Error:     return TEXT("! error");
		default:               return TEXT("·");
		}
	}
}

void SEarth4DChatPanel::Construct(const FArguments& InArgs)
{
	SubsystemGetter = InArgs._SubsystemGetter;

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(6, 6, 6, 2)
		[ SNew(STextBlock).Text(LOCTEXT("ChatHdr", "CHAT — natural-language control")).ColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.55f, 0.62f))) ]

		// API-key entry (shown only when no key is configured).
		+ SVerticalBox::Slot().AutoHeight().Padding(6, 2)
		[
			SNew(SBorder)
			.Visibility(this, &SEarth4DChatPanel::GetKeyEntryVisibility)
			.BorderBackgroundColor(FLinearColor(0, 0, 0, 0.25f))
			.Padding(6)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[ SNew(STextBlock).AutoWrapText(true).Text(LOCTEXT("KeyPrompt", "Enter your Anthropic API key to enable chat. It is stored per-user (Config/DefaultEarth4D.ini), never committed.")) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 4)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1)
					[ SAssignNew(KeyBox, SEditableTextBox).IsPassword(true).HintText(LOCTEXT("KeyHint", "sk-ant-…")) ]
					+ SHorizontalBox::Slot().AutoWidth().Padding(4, 0, 0, 0)
					[ SNew(SButton).Text(LOCTEXT("SaveKey", "Save")).OnClicked(this, &SEarth4DChatPanel::OnSaveKey) ]
				]
			]
		]

		// Transcript.
		+ SVerticalBox::Slot().FillHeight(1).Padding(6, 2)
		[
			SNew(SBorder)
			.Visibility(this, &SEarth4DChatPanel::GetChatVisibility)
			.BorderBackgroundColor(FLinearColor(0, 0, 0, 0.25f))
			[
				SAssignNew(TranscriptView, SListView<TSharedPtr<FChatLine>>)
				.ListItemsSource(&Transcript)
				.SelectionMode(ESelectionMode::None)
				.OnGenerateRow(this, &SEarth4DChatPanel::OnGenerateRow)
			]
		]

		// Input row.
		+ SVerticalBox::Slot().AutoHeight().Padding(6, 2, 6, 8)
		[
			SNew(SHorizontalBox)
			.Visibility(this, &SEarth4DChatPanel::GetChatVisibility)
			+ SHorizontalBox::Slot().FillWidth(1)
			[
				SAssignNew(InputBox, SEditableTextBox)
				.HintText(LOCTEXT("InputHint", "e.g. \"make the steel task 12 days and start it on day 20\"  (Enter to send)"))
				.OnTextCommitted(this, &SEarth4DChatPanel::OnInputCommitted)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(4, 0, 0, 0).VAlign(VAlign_Center)
			[
				SNew(SButton)
				.Text(LOCTEXT("Send", "Send"))
				.IsEnabled(this, &SEarth4DChatPanel::IsSendEnabled)
				.OnClicked(this, &SEarth4DChatPanel::OnSend)
			]
		]
	];

	Append(FChatLine::ERole::System, TEXT("Ask in plain language — every change runs through the same command layer as the Gantt."));
}

SEarth4DChatPanel::~SEarth4DChatPanel()
{
	if (Client.IsValid() && NativeEventHandle.IsValid())
		Client->OnNativeEvent.Remove(NativeEventHandle);
}

void SEarth4DChatPanel::EnsureClient()
{
	if (!Client.IsValid())
	{
		Client.Reset(NewObject<UEarth4DChatClient>(GetTransientPackage()));
		NativeEventHandle = Client->OnNativeEvent.AddSP(this, &SEarth4DChatPanel::OnNativeChatEvent);
	}
	// (Re)configure each send so a freshly-entered key / changed world is picked up.
	const UEarth4DSettings* Settings = UEarth4DSettings::Get();
	Client->Model = Settings->ClaudeModel;
	Client->MaxTokens = Settings->ChatMaxTokens;
	Client->Configure(Settings->ClaudeApiKey, ResolveSub());
}

bool SEarth4DChatPanel::HasApiKey() const
{
	return !UEarth4DSettings::Get()->ClaudeApiKey.IsEmpty();
}

EVisibility SEarth4DChatPanel::GetKeyEntryVisibility() const { return HasApiKey() ? EVisibility::Collapsed : EVisibility::Visible; }
EVisibility SEarth4DChatPanel::GetChatVisibility() const { return HasApiKey() ? EVisibility::Visible : EVisibility::Collapsed; }
bool SEarth4DChatPanel::IsSendEnabled() const { return !(Client.IsValid() && Client->bBusy); }

FReply SEarth4DChatPanel::OnSaveKey()
{
	if (KeyBox.IsValid())
	{
		const FString Key = KeyBox->GetText().ToString().TrimStartAndEnd();
		if (!Key.IsEmpty())
		{
			UEarth4DSettings::GetMutable()->SetClaudeApiKey(Key);
			Append(FChatLine::ERole::System, TEXT("API key saved. You can start chatting."));
		}
	}
	return FReply::Handled();
}

void SEarth4DChatPanel::OnInputCommitted(const FText& Text, ETextCommit::Type CommitType)
{
	if (CommitType == ETextCommit::OnEnter) OnSend();
}

FReply SEarth4DChatPanel::OnSend()
{
	if (!InputBox.IsValid()) return FReply::Handled();
	const FString Msg = InputBox->GetText().ToString().TrimStartAndEnd();
	if (Msg.IsEmpty()) return FReply::Handled();
	InputBox->SetText(FText::GetEmpty());

	EnsureClient();
	Append(FChatLine::ERole::User, Msg);
	Client->SendMessage(Msg);
	return FReply::Handled();
}

void SEarth4DChatPanel::OnNativeChatEvent(EEarth4DChatEvent Kind, const FString& Text)
{
	switch (Kind)
	{
	case EEarth4DChatEvent::AssistantText: Append(FChatLine::ERole::Assistant, Text); break;
	case EEarth4DChatEvent::ToolRun:       Append(FChatLine::ERole::Tool, Text); break;
	case EEarth4DChatEvent::Error:         Append(FChatLine::ERole::Error, Text); break;
	default: break; // Busy/Idle drive IsSendEnabled via Client->bBusy.
	}
}

void SEarth4DChatPanel::Append(FChatLine::ERole Role, const FString& Text)
{
	TSharedPtr<FChatLine> Line = MakeShared<FChatLine>();
	Line->Role = Role;
	Line->Text = Text;
	Transcript.Add(Line);
	if (TranscriptView.IsValid())
	{
		TranscriptView->RequestListRefresh();
		TranscriptView->RequestScrollIntoView(Line);
	}
}

TSharedRef<ITableRow> SEarth4DChatPanel::OnGenerateRow(TSharedPtr<FChatLine> Item, const TSharedRef<STableViewBase>& Owner)
{
	return SNew(STableRow<TSharedPtr<FChatLine>>, Owner)
		.Padding(FMargin(6, 3))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[ SNew(STextBlock).Text(FText::FromString(RolePrefix(Item->Role))).ColorAndOpacity(FSlateColor(RoleColor(Item->Role).CopyWithNewOpacity(0.7f))) ]
			+ SVerticalBox::Slot().AutoHeight()
			[ SNew(STextBlock).AutoWrapText(true).Text(FText::FromString(Item->Text)).ColorAndOpacity(FSlateColor(RoleColor(Item->Role))) ]
		];
}

#undef LOCTEXT_NAMESPACE
