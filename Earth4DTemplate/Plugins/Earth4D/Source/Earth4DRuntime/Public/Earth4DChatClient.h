// Copyright Earth4D. Licensed for project use.
// In-app natural-language control: a Claude Messages API client with a tool-use
// loop that drives the Earth4D command layer. Works in the packaged standalone
// app (no Claude Code/Desktop dependency).
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Interfaces/IHttpRequest.h"
#include "Earth4DChatClient.generated.h"

class UEarth4DSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEarth4DChatMessage, const FString&, Text);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEarth4DChatToolRun, const FString&, ToolSummary);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEarth4DChatError, const FString&, Error);

/** Native event kinds for C++ (Slate) listeners that can't bind dynamic delegates. */
enum class EEarth4DChatEvent : uint8 { AssistantText, ToolRun, Error, Busy, Idle };
DECLARE_MULTICAST_DELEGATE_TwoParams(FEarth4DNativeChatEvent, EEarth4DChatEvent /*Kind*/, const FString& /*Text*/);

UCLASS(BlueprintType)
class EARTH4DRUNTIME_API UEarth4DChatClient : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Earth4D|Chat") FString Model = TEXT("claude-opus-4-8");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Earth4D|Chat") int32 MaxTokens = 2048;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Earth4D|Chat") int32 MaxToolRounds = 8;

	/** Streamed-back assistant text, tool-run notices, and errors for the UI. */
	UPROPERTY(BlueprintAssignable) FEarth4DChatMessage OnAssistantText;
	UPROPERTY(BlueprintAssignable) FEarth4DChatToolRun OnToolRun;
	UPROPERTY(BlueprintAssignable) FEarth4DChatError OnError;

	/** Native (C++/Slate) event stream mirroring the Blueprint delegates above. */
	FEarth4DNativeChatEvent OnNativeEvent;

	/** True while a turn (or tool-use loop) is in flight. */
	UPROPERTY(BlueprintReadOnly, Category = "Earth4D|Chat") bool bBusy = false;

	UFUNCTION(BlueprintCallable, Category = "Earth4D|Chat") void Configure(const FString& InApiKey, UEarth4DSubsystem* InSubsystem);
	/** Send a user instruction; drives the tool-use loop until a final answer. */
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Chat") void SendMessage(const FString& UserText);
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Chat") void ResetConversation();

private:
	FString ApiKey;
	UPROPERTY() UEarth4DSubsystem* Subsystem = nullptr;

	/** Conversation as Anthropic message objects (role + content blocks). */
	TArray<TSharedPtr<class FJsonValue>> Messages;
	int32 RoundsLeft = 0;

	void PostTurn();
	void OnResponse(FHttpRequestPtr Req, FHttpResponsePtr Resp, bool bOk);
	FString BuildSystemPrompt() const;
	/** End the current turn: clears bBusy, reports an optional error, fires Idle. */
	void Finish(const FString& Error = FString());
};
