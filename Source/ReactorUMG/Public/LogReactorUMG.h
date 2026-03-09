/**
 * @file LogReactorUMG.h
 * @brief Declares the shared log category used throughout the
 *        ReactorUMG plugin for all UE_LOG output.
 *
 * Usage:
 * @code
 *   UE_LOG(LogReactorUMG, Display, TEXT("Widget initialized"));
 * @endcode
 */

#pragma once

#include "CoreMinimal.h"

/** Global log category for the ReactorUMG plugin. */
REACTORUMG_API DECLARE_LOG_CATEGORY_EXTERN(LogReactorUMG, Log, All);