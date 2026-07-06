// Copyright Earth4D. Licensed for project use.
#include "Earth4DMcpServer.h"
#include "Earth4DSubsystem.h"
#include "Earth4DTools.h"
#include "HttpServerModule.h"
#include "HttpServerResponse.h"
#include "IHttpRouter.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Editor.h"
#include "Engine/World.h"

UEarth4DSubsystem* FEarth4DMcpServer::ResolveSubsystem() const
{
	if (GEditor)
	{
		if (GEditor->PlayWorld) return GEditor->PlayWorld->GetSubsystem<UEarth4DSubsystem>();
		if (UWorld* W = GEditor->GetEditorWorldContext().World())
			return W->GetSubsystem<UEarth4DSubsystem>();
	}
	return nullptr;
}

namespace
{
	FString JsonToString(const TSharedRef<FJsonObject>& Obj)
	{
		FString Out;
		TSharedRef<TJsonWriter<>> W = TJsonWriterFactory<>::Create(&Out);
		FJsonSerializer::Serialize(Obj, W);
		return Out;
	}

	// Build a JSON-RPC 2.0 result envelope: {"jsonrpc":"2.0","id":<id>,"result":<result>}.
	FString RpcResult(const TSharedPtr<FJsonValue>& Id, const TSharedPtr<FJsonObject>& Result)
	{
		TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetStringField(TEXT("jsonrpc"), TEXT("2.0"));
		Root->SetField(TEXT("id"), Id.IsValid() ? Id : MakeShared<FJsonValueNull>());
		Root->SetObjectField(TEXT("result"), Result);
		return JsonToString(Root);
	}

	FString RpcError(const TSharedPtr<FJsonValue>& Id, int32 Code, const FString& Message)
	{
		TSharedRef<FJsonObject> Err = MakeShared<FJsonObject>();
		Err->SetNumberField(TEXT("code"), Code);
		Err->SetStringField(TEXT("message"), Message);
		TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetStringField(TEXT("jsonrpc"), TEXT("2.0"));
		Root->SetField(TEXT("id"), Id.IsValid() ? Id : MakeShared<FJsonValueNull>());
		Root->SetObjectField(TEXT("error"), Err);
		return JsonToString(Root);
	}
}

FString FEarth4DMcpServer::HandleJsonRpc(const FString& Body) const
{
	TSharedPtr<FJsonObject> Req;
	TSharedRef<TJsonReader<>> R = TJsonReaderFactory<>::Create(Body);
	if (!FJsonSerializer::Deserialize(R, Req) || !Req.IsValid())
		return RpcError(nullptr, -32700, TEXT("Parse error"));

	const TSharedPtr<FJsonValue> Id = Req->TryGetField(TEXT("id"));
	const FString Method = Req->GetStringField(TEXT("method"));
	const TSharedPtr<FJsonObject>* ParamsPtr = nullptr;
	Req->TryGetObjectField(TEXT("params"), ParamsPtr);
	const TSharedPtr<FJsonObject> Params = ParamsPtr ? *ParamsPtr : MakeShared<FJsonObject>();

	// Notifications (no id) get no response body.
	if (!Id.IsValid() && Method.StartsWith(TEXT("notifications/"))) return FString();

	if (Method == TEXT("initialize"))
	{
		TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetStringField(TEXT("protocolVersion"), TEXT("2024-11-05"));
		TSharedRef<FJsonObject> Caps = MakeShared<FJsonObject>();
		Caps->SetObjectField(TEXT("tools"), MakeShared<FJsonObject>());
		Result->SetObjectField(TEXT("capabilities"), Caps);
		TSharedRef<FJsonObject> Info = MakeShared<FJsonObject>();
		Info->SetStringField(TEXT("name"), TEXT("earth4d"));
		Info->SetStringField(TEXT("version"), TEXT("0.1.0"));
		Result->SetObjectField(TEXT("serverInfo"), Info);
		return RpcResult(Id, Result);
	}
	if (Method == TEXT("tools/list"))
	{
		TArray<TSharedPtr<FJsonValue>> Tools;
		TSharedRef<TJsonReader<>> TR = TJsonReaderFactory<>::Create(Earth4DTools::GetToolsJson());
		FJsonSerializer::Deserialize(TR, Tools);
		TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetArrayField(TEXT("tools"), Tools);
		return RpcResult(Id, Result);
	}
	if (Method == TEXT("tools/call"))
	{
		const FString Name = Params->GetStringField(TEXT("name"));
		const TSharedPtr<FJsonObject>* ArgsPtr = nullptr;
		Params->TryGetObjectField(TEXT("arguments"), ArgsPtr);
		const TSharedPtr<FJsonObject> ToolArgs = ArgsPtr ? *ArgsPtr : MakeShared<FJsonObject>();
		const FString ResultText = Earth4DTools::Dispatch(Name, ToolArgs, ResolveSubsystem());

		// MCP tools/call result: { content: [ {type:"text", text:...} ], isError:false }.
		TSharedRef<FJsonObject> TextBlock = MakeShared<FJsonObject>();
		TextBlock->SetStringField(TEXT("type"), TEXT("text"));
		TextBlock->SetStringField(TEXT("text"), ResultText);
		TArray<TSharedPtr<FJsonValue>> Content; Content.Add(MakeShared<FJsonValueObject>(TextBlock));
		TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetArrayField(TEXT("content"), Content);
		Result->SetBoolField(TEXT("isError"), ResultText.StartsWith(TEXT("error")));
		return RpcResult(Id, Result);
	}

	return RpcError(Id, -32601, FString::Printf(TEXT("Method not found: %s"), *Method));
}

void FEarth4DMcpServer::Start(uint32 Port)
{
	FHttpServerModule& Http = FHttpServerModule::Get();
	Router = Http.GetHttpRouter(Port);
	if (!Router.IsValid()) return;
	BoundPort = Port;

	// MCP JSON-RPC 2.0 over HTTP (initialize / tools/list / tools/call). This is the
	// stable contract; if UE 5.8's first-party MCP host (ModelContextProtocol plugin)
	// is used instead, it registers the SAME Earth4DTools list — the command layer is
	// the constant either way.
	RouteHandles.Add(Router->BindRoute(FHttpPath(TEXT("/mcp")), EHttpServerRequestVerbs::VERB_POST,
		FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete)
		{
			const FString Body(UTF8_TO_TCHAR(reinterpret_cast<const char*>(Req.Body.GetData())), Req.Body.Num());
			const FString Resp = HandleJsonRpc(Body);
			OnComplete(FHttpServerResponse::Create(Resp, TEXT("application/json")));
			return true;
		})));

	// Convenience plain-HTTP routes (handy for curl / quick integration / debugging).
	RouteHandles.Add(Router->BindRoute(FHttpPath(TEXT("/tools")), EHttpServerRequestVerbs::VERB_GET,
		FHttpRequestHandler::CreateLambda([](const FHttpServerRequest&, const FHttpResultCallback& OnComplete)
		{
			OnComplete(FHttpServerResponse::Create(Earth4DTools::GetToolsJson(), TEXT("application/json")));
			return true;
		})));
	RouteHandles.Add(Router->BindRoute(FHttpPath(TEXT("/call")), EHttpServerRequestVerbs::VERB_POST,
		FHttpRequestHandler::CreateLambda([this](const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete)
		{
			const FString Body(UTF8_TO_TCHAR(reinterpret_cast<const char*>(Req.Body.GetData())), Req.Body.Num());
			TSharedPtr<FJsonObject> Root;
			TSharedRef<TJsonReader<>> R = TJsonReaderFactory<>::Create(Body);
			FString Result = TEXT("error: bad request");
			if (FJsonSerializer::Deserialize(R, Root) && Root.IsValid())
			{
				const FString Name = Root->GetStringField(TEXT("name"));
				const TSharedPtr<FJsonObject> Args = Root->HasField(TEXT("arguments"))
					? Root->GetObjectField(TEXT("arguments")) : MakeShared<FJsonObject>();
				Result = Earth4DTools::Dispatch(Name, Args, ResolveSubsystem());
			}
			OnComplete(FHttpServerResponse::Create(Result, TEXT("text/plain")));
			return true;
		})));

	Http.StartAllListeners();
	UE_LOG(LogTemp, Display, TEXT("[Earth4D] MCP server listening on http://127.0.0.1:%u/mcp (JSON-RPC) + /tools, /call."), Port);
}

void FEarth4DMcpServer::Stop()
{
	if (Router.IsValid())
		for (const FHttpRouteHandle& H : RouteHandles)
			if (H.IsValid()) Router->UnbindRoute(H);
	RouteHandles.Reset();
	Router.Reset();
}
