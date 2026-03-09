/**
 * @file LatentActionState.h
 * @brief Helper for bridging Unreal latent actions to JS callbacks.
 *
 * Wraps the standard FLatentActionInfo pattern so that TypeScript code
 * can start a Blueprint latent action and receive a callback when it
 * completes, without needing to understand the underlying link-ID
 * mechanism.
 */

#pragma once

#include "CoreMinimal.h"
#include "Engine/LatentActionManager.h"
#include "UObject/NoExportTypes.h"
#include "LatentActionState.generated.h"

/** Dynamic delegate fired when the associated latent action completes. */
DECLARE_DYNAMIC_DELEGATE(FLatentActionCallback);

/**
 * @class ULatentActionState
 * @brief Bridges Unreal's latent-action completion callback into a
 *        simple dynamic delegate that PuerTS/JS can bind.
 *
 * Usage from TypeScript:
 * @code
 *   const state = new ULatentActionState();
 *   state.LatentActionCallback.Bind(() => console.log("done!"));
 *   SomeLatentFunction(state.GetLatentActionInfo());
 * @endcode
 */
UCLASS()
class REACTORUMG_API ULatentActionState : public UObject
{
	GENERATED_BODY()
	
public:
	/** @brief Delegate that JS/TS can bind to receive completion notification. */
	UPROPERTY()
	FLatentActionCallback LatentActionCallback;

	/**
	 * @brief Internal callback target for the latent action system.
	 *
	 * Do not call directly -- this is wired as the ExecutionFunction
	 * in the FLatentActionInfo returned by GetLatentActionInfo().
	 *
	 * @param LinkID  The link ID from the latent action (unused here).
	 */
	UFUNCTION()
	void OnLatentActionCompleted(int32 LinkID);

	/**
	 * @brief Create a FLatentActionInfo that will fire LatentActionCallback
	 *        when the latent action completes.
	 * @return A ready-to-use FLatentActionInfo bound to this object.
	 */
	UFUNCTION(BlueprintCallable, Category="Reactor|LatentState")
	FLatentActionInfo GetLatentActionInfo();
};
