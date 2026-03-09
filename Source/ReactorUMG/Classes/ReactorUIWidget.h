/**
 * @file ReactorUIWidget.h
 * @brief Core widget class that hosts a React-driven UMG widget tree.
 *
 * UReactorUIWidget is the entry point for every React UI surface in Unreal.
 * It owns a PuerTS JavaScript environment, boots the user's React script,
 * and bridges the React reconciler output back into the Slate/UMG layer.
 *
 * Lifecycle:
 *   Initialize() -> SetNewWidgetTree() -> RunScriptToInitWidgetTree()
 *   NativeTick()  (per-frame, flushes batched property syncs)
 *   BeginDestroy() -> tears down bridge callers and JS environment
 */

#pragma once

#include "CustomJSArg.h"
#include "JsEnv.h"
#include "Blueprint/UserWidget.h"
#include "ReactorUIWidget.generated.h"

/**
 * @class UReactorUIWidget
 * @brief A UUserWidget subclass whose visual tree is built and managed
 *        entirely by a React/TypeScript script executed via PuerTS.
 *
 * Each instance creates its own JavaScript environment, executes the
 * associated launch script, and wires a JsBridgeCaller so that the
 * React reconciler can push widget mutations back into the UMG tree.
 *
 * The widget participates in Unreal's tick pipeline via NativeTick(),
 * which is used to flush batched property synchronisations queued by
 * the reconciler during its commit phase.
 */
UCLASS(BlueprintType)
class REACTORUMG_API UReactorUIWidget : public UUserWidget
{
	GENERATED_UCLASS_BODY()

public:
	/** @brief Standard UUserWidget initialisation; boots the React script. */
	virtual bool Initialize() override;

	/**
	 * @brief Cleanup callback invoked by the garbage collector.
	 *
	 * Removes the JsBridgeCaller associated with this widget's script,
	 * releases the JS environment back to the pool, and nulls all
	 * pointers to prevent dangling references.
	 */
	virtual void BeginDestroy() override;

	/**
	 * @brief Per-frame tick callback.
	 *
	 * Used to flush batched widget property synchronizations that were
	 * queued during the React reconciler's commit phase.  This ensures
	 * all Slate-level property pushes happen exactly once per frame,
	 * in bulk, rather than per-property-change.
	 *
	 * @param MyGeometry  The allocated geometry for this widget.
	 * @param InDeltaTime Seconds elapsed since the last frame.
	 */
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

protected:
	/**
	 * @brief Creates a fresh WidgetTree and runs the launch script.
	 *
	 * Called once during initialization. Replaces the inherited
	 * WidgetTree with a transient one and looks up the script path
	 * from the associated ReactorUMGBlueprintGeneratedClass.
	 */
	void SetNewWidgetTree();

#if WITH_EDITOR
	/** @brief Palette category for the UMG designer. */
	virtual const FText GetPaletteCategory() override;
#endif

private:
	/**
	 * @brief Boot the PuerTS environment and execute the launch script.
	 *
	 * Allocates a JsBridgeCaller, passes it + the WidgetTree as arguments
	 * to the JS environment, then executes the MainCaller delegate so
	 * the React tree can mount into the WidgetTree.
	 */
	void RunScriptToInitWidgetTree();

	/**
	 * @brief Return the JS environment to the runtime pool.
	 *
	 * Called after script initialization completes and also during
	 * destruction to ensure the environment is not leaked.
	 */
	void ReleaseJsEnv();

	/** Custom argument object passed to the JS environment on startup. */
	UPROPERTY()
	TObjectPtr<UCustomJSArg> CustomJSArg;
	
	/** Relative path to the compiled JS entry point. */
	FString LaunchScriptPath;

	/** Root directory of the TypeScript project. */
	FString TsProjectDir;

	/** Relative directory from the TS project root to the script home. */
	FString TsScriptHomeRelativeDir;

	/** Shared pointer to the PuerTS JS environment for this widget. */
	TSharedPtr<puerts::FJsEnv> JsEnv;

	/** Guards one-shot initialization so SetNewWidgetTree runs only once. */
	bool bWidgetTreeInitialized;
};

