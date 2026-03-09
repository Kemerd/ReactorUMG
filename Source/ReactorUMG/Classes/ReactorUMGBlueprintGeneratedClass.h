/**
 * @file ReactorUMGBlueprintGeneratedClass.h
 * @brief Generated class that carries per-asset script path metadata.
 *
 * The editor-side blueprint compiler (ReactorBlueprintCompilerContext)
 * populates these properties during asset compilation so that the
 * runtime can locate the correct TypeScript entry point without
 * parsing tsconfig at every widget construction.
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/Package.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "ReactorUMGBlueprintGeneratedClass.generated.h"

/**
 * @class UReactorUMGBlueprintGeneratedClass
 * @brief Extends UWidgetBlueprintGeneratedClass with script-path
 *        metadata set by the blueprint compiler.
 *
 * Each ReactorUMG widget blueprint produces one of these classes.
 * At runtime, UReactorUIWidget reads MainScriptPath and the project
 * directories to boot the correct JavaScript entry point.
 */
UCLASS()
class REACTORUMG_API UReactorUMGBlueprintGeneratedClass : public UWidgetBlueprintGeneratedClass
{
	GENERATED_UCLASS_BODY()

public:
	/**
	 * @brief Post-load hook that clears stale property data in the editor.
	 *
	 * In non-game editor sessions, clears all ExcludeSuper fields on this
	 * class to ensure the compiler can re-populate them cleanly.
	 */
	virtual void PostLoad() override;

	/** @brief Absolute path to the compiled JS entry point for this widget. */
	UPROPERTY(BlueprintReadOnly, Category="ReactorUMG")
	FString MainScriptPath;

	/** @brief Root directory of the TypeScript project. */
	UPROPERTY(BlueprintReadOnly, Category="ReactorUMG")
	FString TsProjectDir;

	/** @brief Absolute path to the script home directory. */
	UPROPERTY(BlueprintReadOnly, Category="ReactorUMG")
	FString TsScriptHomeFullDir;

	/** @brief Script home directory relative to the TS project root. */
	UPROPERTY(BlueprintReadOnly, Category="ReactorUMG")
	FString TsScriptHomeRelativeDir;

	/** @brief Human-readable name of the widget (for logging / UI). */
	UPROPERTY(BlueprintReadOnly, Category="ReactorUMG")
	FString WidgetName;
};
