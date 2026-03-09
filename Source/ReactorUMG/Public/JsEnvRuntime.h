/**
 * @file JsEnvRuntime.h
 * @brief PuerTS JavaScript environment pool and hot-reload system.
 *
 * Manages a pool of FJsEnv instances that are handed out to
 * UReactorUIWidget instances on demand.  Provides hot-reload
 * capabilities by tracking file content hashes and only reloading
 * modules whose source has actually changed on disk.
 */

#pragma once

#include "JsEnv.h"
#include "JSLogger.h"

/**
 * @class FReactorUMGJSLogger
 * @brief Custom PuerTS logger that routes JS console output to UE_LOG
 *        and the editor Message Log panel.
 */
class FReactorUMGJSLogger : public PUERTS_NAMESPACE::ILogger
{
public:
	/**
	 * @param CategoryName  Name shown in the Message Log tab (editor only).
	 */
	explicit FReactorUMGJSLogger(const FString& CategoryName = TEXT("ReactorUMG"))
		: MessageLogCategoryName(CategoryName)
	{
	}
	
	virtual ~FReactorUMGJSLogger() {}

	void Log(const FString& Message) const override;
	void Info(const FString& Message) const override;
	void Warn(const FString& Message) const override;
	void Error(const FString& Message) const override;

private:
	/** Category name used for the editor Message Log grouping. */
	FString MessageLogCategoryName;
};

/**
 * @class FJsEnvRuntime
 * @brief Singleton that manages a pool of PuerTS JavaScript environments
 *        and provides hot-reload infrastructure for development iteration.
 *
 * Environment pool:
 *   The pool is pre-warmed at construction with a configurable number
 *   of FJsEnv instances.  Widgets call GetFreeJsEnv() to borrow one,
 *   execute their script, and return it via ReleaseJsEnv().
 *
 * Hot-reload:
 *   HotReloadChangedScripts() scans the script home directory, computes
 *   MD5 hashes for every .js/.css file, compares against the last-known
 *   hashes, and only reloads modules that have actually changed.  This
 *   avoids the heavy cost of a full environment rebuild during iteration.
 */
class FJsEnvRuntime
{
public:
	/** @brief Access the process-wide singleton. */
	REACTORUMG_API static FJsEnvRuntime& GetInstance()
	{
		static FJsEnvRuntime Instance;
		return Instance;
	}

	~FJsEnvRuntime();

	/**
	 * @brief Borrow a free JS environment from the pool.
	 * @return A shared pointer to an idle FJsEnv, or nullptr if all are busy.
	 */
	REACTORUMG_API TSharedPtr<PUERTS_NAMESPACE::FJsEnv> GetFreeJsEnv();
		
	/**
	 * @brief Execute a JavaScript entry-point script in the given environment.
	 * @param JsEnv      The borrowed JS environment.
	 * @param Script     Module path of the script to start.
	 * @param Arguments  Named UObject arguments passed into the JS global scope.
	 * @return           True if Start() succeeded.
	 */
	REACTORUMG_API bool StartJavaScript(const TSharedPtr<PUERTS_NAMESPACE::FJsEnv>& JsEnv, const FString& Script, const TArray<TPair<FString, UObject*>>& Arguments) const;

	/**
	 * @brief Check whether a script file exists on disk (with .js fallback).
	 * @param Script  Path to check.
	 * @return        True if the file is found.
	 */
	REACTORUMG_API bool CheckScriptLegal(const FString& Script) const;

	/**
	 * @brief Return a JS environment to the pool so it can be reused.
	 * @param JsEnv  The environment to release.
	 */
	REACTORUMG_API void ReleaseJsEnv(TSharedPtr<PUERTS_NAMESPACE::FJsEnv> JsEnv);

	/**
	 * @brief Destroy all pooled environments and recreate them.
	 *
	 * Use this when you need a clean slate (e.g. after major plugin
	 * config changes). Prefer HotReloadChangedScripts() for normal
	 * development iteration.
	 */
	REACTORUMG_API void RebuildRuntimePool();

	/**
	 * @brief Full reload: reload all scripts under ScriptHomeDir,
	 *        then re-execute the main entry point.
	 *
	 * @param JSContentDir   Absolute path to the JS output directory.
	 * @param ScriptHomeDir  Relative subdirectory containing the scripts.
	 * @param MainJsScript   The main entry-point script to re-execute.
	 * @param Arguments      Named UObject arguments for the script.
	 */
	REACTORUMG_API void RestartJsScripts(const FString& JSContentDir, const FString& ScriptHomeDir, const FString& MainJsScript, const TArray<TPair<FString, UObject*>>& Arguments);

	/**
	 * @brief Incremental hot-reload: only reload modules whose file
	 *        content has changed since the last reload.
	 *
	 * Computes MD5 hashes for all .js and .css files under ScriptHomeDir
	 * and compares them against the cached hashes from the previous call.
	 * Only changed files are pushed through ReloadModule/ForceReloadJsFile.
	 *
	 * @param JSContentDir   Absolute path to the JS output directory.
	 * @param ScriptHomeDir  Relative subdirectory containing the scripts.
	 * @param MainJsScript   The main entry-point script to re-execute.
	 * @param Arguments      Named UObject arguments for the script.
	 * @return               Number of modules that were actually reloaded.
	 */
	REACTORUMG_API int32 HotReloadChangedScripts(const FString& JSContentDir, const FString& ScriptHomeDir, const FString& MainJsScript, const TArray<TPair<FString, UObject*>>& Arguments);

	/**
	 * @brief Clear the cached file hashes so the next hot-reload
	 *        treats every file as changed.
	 */
	REACTORUMG_API void InvalidateFileHashCache();

private:
	/**
	 * @param EnvPoolSize  Number of FJsEnv instances to pre-create.
	 */
	FJsEnvRuntime(int32 EnvPoolSize = 1);

	/**
	 * @brief Compute an MD5 hash of a file's contents.
	 * @param FilePath  Absolute path to the file.
	 * @return          Hex-encoded MD5 string, or empty on failure.
	 */
	static FString ComputeFileHash(const FString& FilePath);

	/**
	 * Pool of JS environments.  Value 0 = idle, 1 = in use.
	 */
	TMap<TSharedPtr<PUERTS_NAMESPACE::FJsEnv>, int32> JsRuntimeEnvPool;

	/** Shared logger routed to UE_LOG and editor Message Log. */
	std::shared_ptr<FReactorUMGJSLogger> ReactorUmgLogger;

	/** Number of environments to keep in the pool. */
	int32 EnvPoolSize;

	/**
	 * Cache of file path -> MD5 hash from the last hot-reload pass.
	 * Used to detect which files actually changed on disk.
	 */
	TMap<FString, FString> LastKnownFileHashes;
};
