// Copyright Earth4D. Licensed for project use.
// The in-app natural-language control surface (ARCHITECTURE.md §5a). Owns a
// UEarth4DChatClient, grounds it in the live schedule, and runs the Claude tool-use
// loop against the command layer — so a typed instruction and a Gantt drag are the
// same mutation. Also hosts the API-key entry when none is configured.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "UObject/StrongObjectPtr.h"
#include "Earth4DChatClient.h"

class UEarth4DSubsystem;
class SEditableTextBox;

class SEarth4DChatPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SEarth4DChatPanel) {}
		SLATE_ARGUMENT(TFunction<UEarth4DSubsystem*()>, SubsystemGetter)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SEarth4DChatPanel() override;

private:
	struct FChatLine
	{
		enum class ERole : uint8 { User, Assistant, Tool, Error, System } Role;
		FString Text;
	};

	UEarth4DSubsystem* ResolveSub() const { return SubsystemGetter ? SubsystemGetter() : nullptr; }
	void EnsureClient();

	TSharedRef<ITableRow> OnGenerateRow(TSharedPtr<FChatLine> Item, const TSharedRef<STableViewBase>& Owner);
	void Append(FChatLine::ERole Role, const FString& Text);
	void OnNativeChatEvent(EEarth4DChatEvent Kind, const FString& Text);

	FReply OnSend();
	void OnInputCommitted(const FText& Text, ETextCommit::Type CommitType);
	FReply OnSaveKey();
	bool HasApiKey() const;
	EVisibility GetKeyEntryVisibility() const;
	EVisibility GetChatVisibility() const;
	bool IsSendEnabled() const;

	TFunction<UEarth4DSubsystem*()> SubsystemGetter;
	TStrongObjectPtr<UEarth4DChatClient> Client;
	FDelegateHandle NativeEventHandle;

	TArray<TSharedPtr<FChatLine>> Transcript;
	TSharedPtr<SListView<TSharedPtr<FChatLine>>> TranscriptView;
	TSharedPtr<SEditableTextBox> InputBox;
	TSharedPtr<SEditableTextBox> KeyBox;
};
