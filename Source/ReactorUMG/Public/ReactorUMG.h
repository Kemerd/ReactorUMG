/**
 * @file ReactorUMG.h
 * @brief Module interface for the ReactorUMG runtime plugin.
 *
 * This is the top-level module that registers the ReactorUMG runtime
 * with the Unreal module system.  Startup and shutdown hooks are
 * available for one-time initialisation (e.g. type generation) and
 * teardown of global resources.
 */

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * @class FReactorUMGModule
 * @brief IModuleInterface implementation for the ReactorUMG runtime module.
 *
 * Currently lightweight -- future work may use StartupModule() to
 * auto-generate TypeScript type declaration files.
 */
class FReactorUMGModule : public IModuleInterface
{
public:
	/** @brief Called when the module is loaded into memory. */
	virtual void StartupModule() override;

	/** @brief Called when the module is unloaded from memory. */
	virtual void ShutdownModule() override;
};
