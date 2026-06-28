// Copyright Earth4D. Licensed for project use.
#include "Earth4DChatClient.h"
#include "Earth4DSubsystem.h"
#include "Earth4DTools.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

void UEarth4DChatClient::Configure(const FString& InApiKey, UEarth4DSubsystem* InSubsystem)
{
	ApiKey = InApiKey;
	Subsystem = InSubsystem;
}

void UEarth4DChatClient::ResetConversation()
{
	Messages.Reset();
}

FString UEarth4DChatClient::BuildSystemPrompt() const
{
	const FString Summary = Subsystem ? Subsystem->GetScheduleSummary() : TEXT("(no schedule)");
	return FString::Printf(TEXT(
		"You control Earth4D, a 4D construction-sequencing tool inside Unreal Engine. "
		"Use the provided tools to modify the schedule: task durations/windows, animation style and direction, "
		"element assignment, stages, and playback. Prefer find_elements to resolve names to ids. "
		"Make the requested change with tools, then briefly confirm what you did. "
		"Current project state:\n%s"), *Summary);
}

static TSharedPtr<FJsonObject> TextBlock(const FString& Role, const FString& Text)
{
	TSharedPtr<FJsonObject> Msg = MakeShared<FJsonObject>();
	Msg->SetStringField(TEXT("role"), Role);
	Msg->SetStringField(TEXT("content"), Text);
	return Msg;
}

void UEarth4DChatClient::SendMessage(const FString& UserText)
{
	if (ApiKey.IsEmpty())
	{
		OnError.Broadcast(TEXT("No API key configured."));
		OnNativeEvent.Broadcast(EEarth4DChatEvent::Error, TEXT("No API key configured."));
		return;
	}
	if (bBusy) { OnNativeEvent.Broadcast(EEarth4DChatEvent::Error, TEXT("Still working on the previous request…")); return; }
	Messages.Add(MakeShared<FJsonValueObject>(TextBlock(TEXT("user"), UserText)));
	RoundsLeft = MaxToolRounds;
	bBusy = true;
	OnNativeEvent.Broadcast(EEarth4DChatEvent::Busy, FString());
	PostTurn();
}

void UEarth4DChatClient::Finish(const FString& Error)
{
	bBusy = false;
	if (!Error.IsEmpty())
	{
		OnError.Broadcast(Error);
		OnNativeEvent.Broadcast(EEarth4DChatEvent::Error, Error);
	}
	OnNativeEvent.Broadcast(EEarth4DChatEvent::Idle, FString());
}

void UEarth4DChatClient::PostTurn()
{
	// Build request body: model, max_tokens, system, tools, messages.
	TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("model"), Model);
	Body->SetNumberField(TEXT("max_tokens"), MaxTokens);
	Body->SetStringField(TEXT("system"), BuildSystemPrompt());
	Body->SetArrayField(TEXT("messages"), Messages);

	// tools (parsed from the shared schema)
	TArray<TSharedPtr<FJsonValue>> Tools;
	TSharedRef<TJsonReader<>> TR = TJsonReaderFactory<>::Create(Earth4DTools::GetToolsJson());
	FJsonSerializer::Deserialize(TR, Tools);
	Body->SetArrayField(TEXT("tools"), Tools);

	FString Payload;
	TSharedRef<TJsonWriter<>> W = TJsonWriterFactory<>::Create(&Payload);
	FJsonSerializer::Serialize(Body.ToSharedRef(), W);

	FHttpRequestRef Req = FHttpModule::Get().CreateRequest();
	Req->SetURL(TEXT("https://api.anthropic.com/v1/messages"));
	Req->SetVerb(TEXT("POST"));
	Req->SetHeader(TEXT("content-type"), TEXT("application/json"));
	Req->SetHeader(TEXT("x-api-key"), ApiKey);
	Req->SetHeader(TEXT("anthropic-version"), TEXT("2023-06-01"));
	Req->SetContentAsString(Payload);
	Req->OnProcessRequestComplete().BindUObject(this, &UEarth4DChatClient::OnResponse);
	Req->ProcessRequest();
}

void UEarth4DChatClient::OnResponse(FHttpRequestPtr Req, FHttpResponsePtr Resp, bool bOk)
{
	if (!bOk || !Resp.IsValid()) { Finish(TEXT("Network error")); return; }
	if (Resp->GetResponseCode() >= 300)
	{
		Finish(FString::Printf(TEXT("API %d: %s"), Resp->GetResponseCode(), *Resp->GetContentAsString()));
		return;
	}

	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> R = TJsonReaderFactory<>::Create(Resp->GetContentAsString());
	if (!FJsonSerializer::Deserialize(R, Root) || !Root.IsValid()) { Finish(TEXT("Bad response")); return; }

	const TArray<TSharedPtr<FJsonValue>>* Content;
	if (!Root->TryGetArrayField(TEXT("content"), Content)) { Finish(TEXT("No content")); return; }

	// Record the assistant turn verbatim (needed so tool_result references resolve).
	TSharedPtr<FJsonObject> AssistantMsg = MakeShared<FJsonObject>();
	AssistantMsg->SetStringField(TEXT("role"), TEXT("assistant"));
	AssistantMsg->SetArrayField(TEXT("content"), *Content);
	Messages.Add(MakeShared<FJsonValueObject>(AssistantMsg));

	// Walk content: surface text, collect tool_use blocks.
	TArray<TSharedPtr<FJsonValue>> ToolResults;
	for (const TSharedPtr<FJsonValue>& Block : *Content)
	{
		const TSharedPtr<FJsonObject> B = Block->AsObject();
		if (!B) continue;
		const FString BType = B->GetStringField(TEXT("type"));
		if (BType == TEXT("text"))
		{
			const FString Text = B->GetStringField(TEXT("text"));
			OnAssistantText.Broadcast(Text);
			OnNativeEvent.Broadcast(EEarth4DChatEvent::AssistantText, Text);
		}
		else if (BType == TEXT("tool_use"))
		{
			const FString Name = B->GetStringField(TEXT("name"));
			const FString Id = B->GetStringField(TEXT("id"));
			const TSharedPtr<FJsonObject> Input = B->GetObjectField(TEXT("input"));
			const FString Result = Earth4DTools::Dispatch(Name, Input, Subsystem);
			const FString Summary = FString::Printf(TEXT("%s → %s"), *Name, *Result);
			OnToolRun.Broadcast(Summary);
			OnNativeEvent.Broadcast(EEarth4DChatEvent::ToolRun, Summary);

			TSharedPtr<FJsonObject> TR = MakeShared<FJsonObject>();
			TR->SetStringField(TEXT("type"), TEXT("tool_result"));
			TR->SetStringField(TEXT("tool_use_id"), Id);
			TR->SetStringField(TEXT("content"), Result);
			ToolResults.Add(MakeShared<FJsonValueObject>(TR));
		}
	}

	const FString StopReason = Root->GetStringField(TEXT("stop_reason"));
	if (StopReason == TEXT("tool_use") && ToolResults.Num() > 0 && RoundsLeft-- > 0)
	{
		// Feed tool results back as a user turn and continue the loop.
		TSharedPtr<FJsonObject> ToolMsg = MakeShared<FJsonObject>();
		ToolMsg->SetStringField(TEXT("role"), TEXT("user"));
		ToolMsg->SetArrayField(TEXT("content"), ToolResults);
		Messages.Add(MakeShared<FJsonValueObject>(ToolMsg));
		PostTurn();
	}
	else
	{
		// Final answer (or the tool-use loop hit its round cap): the turn is done.
		Finish();
	}
}
