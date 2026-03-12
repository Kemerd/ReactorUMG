#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FAutoMixinEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	/** Registers a toolbar button in the Blueprint Editor. */
	void RegistrationButton() const;

	/** Handles the toolbar button press. */
	static void ButtonPressed();

	/** Returns the currently active Blueprint in the editor. */
	static UBlueprint* GetActiveBlueprint();

	/** Registers a context menu entry in the Content Browser. */
	void RegistrationContextButton() const;

	/**
	 * @brief Handles the context menu button press.
	 * @param SelectedAssets  The assets selected in the Content Browser.
	 */
	static void ContextButtonPressed(const TArray<FAssetData>& SelectedAssets);
	
	/** Returns the name of the Slate style set used by this module. */
	static FName GetStyleName();

	/**
	 * @brief Initialises (or re-initialises) the Slate style set.
	 *
	 * Unregisters any existing style set, creates a fresh one with the
	 * MixinIcon brush, and registers it with the Slate style registry.
	 */
	static void InitStyleSet();

	/** Slate style set instance. */
	static TSharedPtr<FSlateStyleSet> StyleSet;
};
