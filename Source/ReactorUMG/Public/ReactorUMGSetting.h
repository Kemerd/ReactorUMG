/**
 * @file ReactorUMGSetting.h
 * @brief Plugin-level developer settings exposed in Project Settings.
 *
 * These settings control where the plugin looks for TypeScript source
 * code and whether it should auto-scaffold a new TS project on first run.
 * They are persisted in DefaultReactorUMG.ini.
 */

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ReactorUMGSetting.generated.h"

/**
 * @class UReactorUMGSetting
 * @brief UDeveloperSettings subclass surfaced under
 *        Project Settings > ReactorUMG.
 */
UCLASS(Config = ReactorUMG, DefaultConfig, meta = (DisplayName = "ReactorUMG"))
class REACTORUMG_API UReactorUMGSetting : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UReactorUMGSetting();

	/**
	 * @brief Root directory of the TypeScript project.
	 *
	 * Relative paths are resolved against the Unreal project directory.
	 * Paths starting with "/Game" are resolved via the package system.
	 * Defaults to "TypeScript".
	 */
	UPROPERTY(EditAnywhere,
		  config,
		  Category = "ReactorUMG",
		  DisplayName = "TypeScript Code Home Directory")
	FString TsScriptProjectDir;

	/**
	 * @brief Whether to auto-generate a TypeScript project scaffold.
	 *
	 * When enabled, the plugin will create tsconfig.json, package.json,
	 * and type declaration files automatically. Disable this if you
	 * manage the TS project structure yourself.
	 */
	UPROPERTY(EditAnywhere, config,
		Category = "ReactorUMG",
		DisplayName = "Generate TypeScript projects automatically",
		meta = (ToolTip =
			"If the option is set, the system will automatically generate a TypeScript project. If not, you need to manually create a TS project, manually generate a type file, and set TsScriptProjectDir to a custom path."))
	bool bAutoGenerateTSProject;

	/** @brief Settings category name for the Project Settings panel. */
	virtual FName GetCategoryName() const override
	{
		return FName(TEXT("ReactorUMG"));
	}
};
