/**
 * @file CustomJSArg.h
 * @brief Lightweight UObject passed to the JavaScript environment as
 *        a custom argument during React widget initialization.
 *
 * Carries runtime configuration flags that the TypeScript side can
 * inspect to decide how to communicate with the C++ backend (e.g.
 * whether to use the delegate-based bridge caller pattern or a
 * direct function call path).
 */

#pragma once

#include "CustomJSArg.generated.h"

/**
 * @class UCustomJSArg
 * @brief Custom argument container injected into the PuerTS JS environment
 *        alongside the WidgetTree and JsBridgeCaller.
 */
UCLASS()
class REACTORUMG_API UCustomJSArg : public UObject
{
	GENERATED_BODY()
	
public:
	UCustomJSArg();

	/**
	 * @brief Whether the JS side should use the JsBridgeCaller delegate
	 *        pattern for mounting the widget tree.
	 *
	 * When true, the JS script is expected to bind a callback on the
	 * MainCaller delegate of the associated UJsBridgeCaller.
	 */
	UPROPERTY(BlueprintType)
	bool bIsUsingBridgeCaller;
};
