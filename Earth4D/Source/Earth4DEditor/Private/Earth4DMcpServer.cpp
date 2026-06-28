// Copyright Earth4D. Licensed for project use.
#include "Earth4DMcpServer.h"
#include "Earth4DSubsystem.h"
#include "Earth4DTools.h"
#include "HttpServerModule.h"
#include "HttpServerResponse.h"
#include "IHttpRouter.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Editor.h"
#include "Engine/World.h"

UEarth4DSubsystem* FEarth4DMcpServer::ResolveSubsystem() const
{
	if (GEditor)
	{
		if (UWorld* W = GEditor->GetEditorWorldContext().World())
		{
			return W->GetSubsystem<UEarth4DSubsystem>();
		}
	}
	return nullptr;
}

void FEarth4DMcpServer::Start(uint32 Port)
{
	FHttpServerModule& Http = FHttpServerModule::Get();
	Router = Http.GetHttpRouter(Port);
	if (!Router.IsValid()) return;
	BoundPort = Port;

	// GET /tools → the shared tool list (MCP "tools/list" payload).
	Router->BindRoute(FHttpPath(TEXT("/tools")), EHttpServerRequestVerbs::VERB_GET,
		FHttpRequestHandler::CreateLambda([](const FHttpServerRequest&, const FHttpResultCallback& OnComplete)
		{
			OnComplete(FHttpServerResponse::Create(Earth4DTools::GetToolsJson(), TEXT("application/json")));
			return true;
		}));

	// POST /call  body: { "name": "...", "arguments": { ... } } → tool result.
	Router->BindRoute(FHttpPath(TEXT("/call")), EHttpServerRequestVerbs::VERB_POST,
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
		}));

	Http.StartAllListeners();
}

void FEarth4DMcpServer::Stop()
{
	// Listeners are torn down with the module; routes drop with the router.
	Router.Reset();
}
