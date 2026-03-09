#include "JsEnvRuntime.h"
#include "Misc/MessageDialog.h"
#include "Logging/MessageLog.h"
#include "LogReactorUMG.h"
#include "ReactorUtils.h"
#include "PuertsSetting.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/SecureHash.h"
#include "HAL/PlatformFileManager.h"

// ===================================================================
//  FReactorUMGJSLogger -- routes JS console output to UE_LOG
// ===================================================================

void FReactorUMGJSLogger::Log(const FString& Message) const
{
	UE_LOG(LogReactorUMG, Log, TEXT("%s"), *Message)
}

void FReactorUMGJSLogger::Error(const FString& Message) const
{
	UE_LOG(LogReactorUMG, Error, TEXT("%s"), *Message)
#if WITH_EDITOR
	FMessageLog(FName(MessageLogCategoryName)).Error()
		->AddToken(FTextToken::Create(FText::FromString(Message)));
#endif
}

void FReactorUMGJSLogger::Warn(const FString& Message) const
{
	UE_LOG(LogReactorUMG, Warning, TEXT("%s"), *Message)
#if WITH_EDITOR
	FMessageLog(FName(MessageLogCategoryName)).Warning()
		->AddToken(FTextToken::Create(FText::FromString(Message)));
#endif
}

void FReactorUMGJSLogger::Info(const FString& Message) const
{
	UE_LOG(LogReactorUMG, Display, TEXT("%s"), *Message)
}

// ===================================================================
//  FJsEnvRuntime -- pool management
// ===================================================================

FJsEnvRuntime::FJsEnvRuntime(int32 InEnvPoolSize)
{
	const int32 DebugPort = GetDefault<UPuertsSetting>()->DebugPort;
	ReactorUmgLogger = std::make_shared<FReactorUMGJSLogger>();
	EnvPoolSize = InEnvPoolSize;

	/* Pre-warm the environment pool */
	for (int32 i = 0; i < EnvPoolSize; i++)
	{
		TSharedPtr<puerts::FJsEnv> Env = MakeShared<puerts::FJsEnv>(
			std::make_unique<puerts::DefaultJSModuleLoader>(TEXT("JavaScript")),
			ReactorUmgLogger, DebugPort + i + 3);
		JsRuntimeEnvPool.Add(Env, 0);
	}
}

FJsEnvRuntime::~FJsEnvRuntime()
{
	JsRuntimeEnvPool.Empty();
	LastKnownFileHashes.Empty();
}

TSharedPtr<puerts::FJsEnv> FJsEnvRuntime::GetFreeJsEnv()
{
	for (auto& Pair : JsRuntimeEnvPool)
	{
		if (Pair.Value == 0)
		{
			Pair.Value = 1;
			return Pair.Key;
		}
	}

	return nullptr;
}

bool FJsEnvRuntime::StartJavaScript(
	const TSharedPtr<puerts::FJsEnv>& JsEnv,
	const FString& Script,
	const TArray<TPair<FString, UObject*>>& Arguments) const
{
	if (JsEnv)
	{
		JsEnv->Start(Script, Arguments);
		return true;
	}

	return false;
}

bool FJsEnvRuntime::CheckScriptLegal(const FString& Script) const
{
	const FString FullPath = Script.EndsWith(TEXT(".js")) ? Script : Script + TEXT(".js");
	
	if (!FPaths::FileExists(FullPath))
	{
		UE_LOG(LogReactorUMG, Error, TEXT("can't find script: %s"), *Script);
		return false;
	}
	
	return true;
}

void FJsEnvRuntime::ReleaseJsEnv(TSharedPtr<puerts::FJsEnv> JsEnv)
{
	for (auto& Pair : JsRuntimeEnvPool)
	{
		if (Pair.Key.Get() == JsEnv.Get())
		{
			JsEnv->Release();
			Pair.Value = 0;
			break;
		}
	}
}

void FJsEnvRuntime::RebuildRuntimePool()
{
	/* Tear down existing environments */
	for (auto& Pair : JsRuntimeEnvPool)
	{
		Pair.Key.Reset();
	}
	JsRuntimeEnvPool.Empty();

	/* Also invalidate the hash cache since we're starting fresh */
	LastKnownFileHashes.Empty();

	/* Recreate the pool */
	const int32 DebugPort = GetDefault<UPuertsSetting>()->DebugPort;
	ReactorUmgLogger = std::make_shared<FReactorUMGJSLogger>();
	for (int32 i = 0; i < EnvPoolSize; i++)
	{
		TSharedPtr<puerts::FJsEnv> Env = MakeShared<puerts::FJsEnv>(
			std::make_unique<puerts::DefaultJSModuleLoader>(TEXT("JavaScript")),
			ReactorUmgLogger, DebugPort + i + 3);
		JsRuntimeEnvPool.Add(Env, 0);
	}
}

// ===================================================================
//  Full script restart (original behaviour, reloads everything)
// ===================================================================

void FJsEnvRuntime::RestartJsScripts(
	const FString& JSContentDir,
	const FString& ScriptHomeDir,
	const FString& MainJsScript,
	const TArray<TPair<FString, UObject*>>& Arguments)
{
	const FString JsScriptHomeDir = FPaths::Combine(JSContentDir, ScriptHomeDir);
	if (JsScriptHomeDir.IsEmpty() || !FPaths::DirectoryExists(JsScriptHomeDir))
	{
		UE_LOG(LogReactorUMG, Warning, TEXT("Script home directory does not exist: %s"), *JsScriptHomeDir);
		return;
	}

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	TArray<FString> FileNames;
	PlatformFile.FindFilesRecursively(FileNames, *JsScriptHomeDir, TEXT(""));

	/* Build the module-name -> absolute-path map for reloadable files */
	TMap<FString, FString> ModuleNames;
	for (FString& SourcePath : FileNames)
	{
		const bool bIsReloadable =
			SourcePath.EndsWith(TEXT(".js")) || SourcePath.EndsWith(TEXT(".css"));
		if (!bIsReloadable)
		{
			continue;
		}
		
		FString RelativePath = SourcePath;
		FPaths::MakePathRelativeTo(RelativePath, *JSContentDir);
		RelativePath.RemoveFromStart(TEXT("JavaScript/"));
		ModuleNames.Add(RelativePath, SourcePath);
	}

	/* Push every module into every environment */
	for (const auto& ModulePair : ModuleNames)
	{
		FString FileContent;
		if (FReactorUtils::ReadFileContent(ModulePair.Value, FileContent))
		{
			for (auto& Pair : JsRuntimeEnvPool)
			{
				auto Env = Pair.Key;
				Env->ReloadModule(FName(*ModulePair.Key), FileContent);
				Env->ForceReloadJsFile(ModulePair.Value);
			}
		}
	}

	/* Re-execute the main entry point in every environment */
	for (auto& Pair : JsRuntimeEnvPool)
	{
		auto Env = Pair.Key;
		Env->Release();
		Env->ForceReloadJsFile(MainJsScript);
		Env->Start(MainJsScript, Arguments);
		Env->Release();
	}
}

// ===================================================================
//  Incremental hot-reload (hash-based change detection)
// ===================================================================

FString FJsEnvRuntime::ComputeFileHash(const FString& FilePath)
{
	TArray<uint8> FileBytes;
	if (!FFileHelper::LoadFileToArray(FileBytes, *FilePath))
	{
		return FString();
	}

	/* Compute MD5 over the raw file bytes */
	uint8 Digest[16];
	FMD5 Md5;
	Md5.Update(FileBytes.GetData(), FileBytes.Num());
	Md5.Final(Digest);

	/* Convert to a hex string for easy comparison */
	return FString::Printf(
		TEXT("%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x"),
		Digest[0], Digest[1], Digest[2],  Digest[3],
		Digest[4], Digest[5], Digest[6],  Digest[7],
		Digest[8], Digest[9], Digest[10], Digest[11],
		Digest[12], Digest[13], Digest[14], Digest[15]);
}

int32 FJsEnvRuntime::HotReloadChangedScripts(
	const FString& JSContentDir,
	const FString& ScriptHomeDir,
	const FString& MainJsScript,
	const TArray<TPair<FString, UObject*>>& Arguments)
{
	const FString JsScriptHomeDir = FPaths::Combine(JSContentDir, ScriptHomeDir);
	if (JsScriptHomeDir.IsEmpty() || !FPaths::DirectoryExists(JsScriptHomeDir))
	{
		UE_LOG(LogReactorUMG, Warning, TEXT("Hot-reload: script directory does not exist: %s"), *JsScriptHomeDir);
		return 0;
	}

	/* Enumerate all reloadable files */
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	TArray<FString> FileNames;
	PlatformFile.FindFilesRecursively(FileNames, *JsScriptHomeDir, TEXT(""));

	int32 ReloadedCount = 0;
	TMap<FString, FString> NewHashes;
	bool bMainScriptChanged = false;

	for (const FString& SourcePath : FileNames)
	{
		const bool bIsReloadable =
			SourcePath.EndsWith(TEXT(".js")) || SourcePath.EndsWith(TEXT(".css"));
		if (!bIsReloadable)
		{
			continue;
		}

		/* Compute the current hash */
		const FString CurrentHash = ComputeFileHash(SourcePath);
		if (CurrentHash.IsEmpty())
		{
			continue;
		}

		NewHashes.Add(SourcePath, CurrentHash);

		/* Compare against the cached hash */
		const FString* PreviousHash = LastKnownFileHashes.Find(SourcePath);
		if (PreviousHash && *PreviousHash == CurrentHash)
		{
			/* File has not changed -- skip it */
			continue;
		}

		/* Compute the relative module name */
		FString RelativePath = SourcePath;
		FPaths::MakePathRelativeTo(RelativePath, *JSContentDir);
		RelativePath.RemoveFromStart(TEXT("JavaScript/"));

		/* Read the file content and push it into every environment */
		FString FileContent;
		if (FReactorUtils::ReadFileContent(SourcePath, FileContent))
		{
			for (auto& Pair : JsRuntimeEnvPool)
			{
				auto Env = Pair.Key;
				Env->ReloadModule(FName(*RelativePath), FileContent);
				Env->ForceReloadJsFile(SourcePath);
			}
			ReloadedCount++;

			/* Check if this is the main entry point */
			if (SourcePath == MainJsScript || RelativePath == MainJsScript)
			{
				bMainScriptChanged = true;
			}

			UE_LOG(LogReactorUMG, Display,
				TEXT("Hot-reload: reloaded module %s"), *RelativePath);
		}
	}

	/* Update the hash cache with the new snapshot */
	LastKnownFileHashes = MoveTemp(NewHashes);

	/*
	 * If the main entry point itself changed, we need to restart it
	 * so that the top-level React tree re-mounts.
	 */
	if (bMainScriptChanged)
	{
		for (auto& Pair : JsRuntimeEnvPool)
		{
			auto Env = Pair.Key;
			Env->Release();
			Env->ForceReloadJsFile(MainJsScript);
			Env->Start(MainJsScript, Arguments);
			Env->Release();
		}

		UE_LOG(LogReactorUMG, Display,
			TEXT("Hot-reload: re-executed main script %s"), *MainJsScript);
	}

	if (ReloadedCount > 0)
	{
		UE_LOG(LogReactorUMG, Display,
			TEXT("Hot-reload complete: %d module(s) reloaded"), ReloadedCount);
	}

	return ReloadedCount;
}

void FJsEnvRuntime::InvalidateFileHashCache()
{
	LastKnownFileHashes.Empty();
	UE_LOG(LogReactorUMG, Display, TEXT("Hot-reload: file hash cache invalidated"));
}