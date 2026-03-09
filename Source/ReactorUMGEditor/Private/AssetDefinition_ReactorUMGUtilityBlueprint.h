#pragma once

#include "ReactorUMGVersion.h"
#include "AssetTypeActions_Base.h"

class AssetDefinition_ReactorUMGUtilityBlueprintAssetTypeActions : public FAssetTypeActions_Base
{
public:
	AssetDefinition_ReactorUMGUtilityBlueprintAssetTypeActions(EAssetTypeCategories::Type Categories);

	// IAssetTypeActions interface
	virtual FText GetName() const override;
	virtual UClass* GetSupportedClass() const override;
	virtual FColor GetTypeColor() const override;
	virtual void OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor) override;
	virtual uint32 GetCategories() override;

#if REACTORUMG_HAS_LEGACY_ASSET_GET_ACTIONS
	virtual void GetActions(const TArray<UObject*>& InObjects, struct FToolMenuSection& Section) override;
#endif
	// End of IAssetTypeActions interface

private:
	EAssetTypeCategories::Type Categories;
	
#if REACTORUMG_HAS_LEGACY_ASSET_GET_ACTIONS
	typedef TArray< TWeakObjectPtr<class UWidgetBlueprint> > FWeakBlueprintPointerArray;
	void ExecuteRun(FWeakBlueprintPointerArray Objects);
#endif
};
