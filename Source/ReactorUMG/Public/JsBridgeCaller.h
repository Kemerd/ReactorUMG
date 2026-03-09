/**
 * @file JsBridgeCaller.h
 * @brief Dynamic delegate bridge between the PuerTS JavaScript environment
 *        and the UMG widget tree.
 *
 * Each React UI surface registers a named bridge caller during script
 * initialisation. The caller's MainCaller delegate is bound on the JS
 * side and invoked from C++ to pass the WidgetTree root object into
 * the React reconciler, completing the mount cycle.
 *
 * Bridge callers are tracked in a static map keyed by script path,
 * ensuring that each UI surface has exactly one active caller.
 */

#pragma once

#include "ReactorUIWidget.h"
#include "JsBridgeCaller.generated.h"

/**
 * @brief Dynamic delegate type invoked to pass a root UObject
 *        (typically a UWidgetTree) into the JavaScript React tree.
 * @param CoreObject  The root container object handed to JS.
 */
DECLARE_DYNAMIC_DELEGATE_OneParam(FJavaScriptMainCaller, UObject*, CoreObject);

/**
 * @class UJsBridgeCaller
 * @brief Statically-managed UObject that holds a JS-bound delegate
 *        for mounting the React widget tree.
 *
 * Lifecycle:
 *   1. AddNewBridgeCaller()          -- allocate and register (AddToRoot)
 *   2. MainCaller is bound in JS     -- JS side binds the delegate
 *   3. ExecuteMainCaller()           -- C++ invokes the delegate
 *   4. RemoveBridgeCaller()          -- unregister and RemoveFromRoot
 *
 * All bridge callers are AddToRoot'd to prevent garbage collection
 * while they are in the static map.
 */
UCLASS(BlueprintType)
class REACTORUMG_API UJsBridgeCaller : public UObject
{
	GENERATED_BODY() 

public:
	/**
	 * @brief Register an externally-allocated bridge caller by name.
	 * @param CallerName  Unique key (typically the script path).
	 * @param Caller      The caller object to register.
	 */
	UFUNCTION(BlueprintCallable, Category="SmartUIWorks | JsBridgeCaller")
	static void RegisterAllocatedBrideCaller(FString CallerName, UJsBridgeCaller* Caller);

	/**
	 * @brief Execute the MainCaller delegate for a named bridge.
	 * @param CallerName     Key that identifies the bridge.
	 * @param RootContainer  The UObject passed to the delegate (usually UWidgetTree).
	 * @return               True if the delegate was bound and executed.
	 */
	UFUNCTION(BlueprintCallable, Category="SmartUIWorks | JsBridgeCaller")
	static bool ExecuteMainCaller(const FString& CallerName, UObject* RootContainer);

	/**
	 * @brief The delegate that JavaScript binds to receive the root container.
	 *
	 * When ExecuteMainCaller() fires, this delegate is what actually
	 * invokes the JS-side callback that mounts the React tree.
	 */
	UPROPERTY()
	FJavaScriptMainCaller MainCaller;

	/**
	 * @brief Check if a bridge caller already exists for the given name.
	 * @param CallerName  Key to look up.
	 * @return            True if a caller is registered under that name.
	 */
	static bool IsExistBridgeCaller(const FString& CallerName);

	/**
	 * @brief Create and register a new bridge caller.
	 *
	 * If a caller already exists for @p CallerName, returns the existing one.
	 * Otherwise creates a new UJsBridgeCaller, AddToRoot's it, and inserts
	 * it into the static map.
	 *
	 * @param CallerName  Unique key for the new caller.
	 * @return            The (possibly pre-existing) bridge caller.
	 */
	static UJsBridgeCaller* AddNewBridgeCaller(const FString& CallerName);

	/**
	 * @brief Unregister and release a bridge caller by name.
	 *
	 * Removes the caller from the static map and calls RemoveFromRoot()
	 * so the garbage collector can reclaim it.
	 *
	 * @param CallerName  Key of the caller to remove.
	 */
	static void RemoveBridgeCaller(const FString& CallerName);

	/**
	 * @brief Remove and release every registered bridge caller.
	 *
	 * Typically called during plugin shutdown or full environment rebuild.
	 */
	static void ClearAllBridgeCaller();

private:
	/**
	 * Static map of caller-name -> UJsBridgeCaller*.
	 * All entries are AddToRoot'd to prevent premature GC.
	 */
	static TMap<FString, UJsBridgeCaller*> SelfHolder;
};
