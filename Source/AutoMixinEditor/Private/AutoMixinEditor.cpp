#include "AutoMixinEditor.h"

#include "BlueprintEditorModule.h"
#include "ContentBrowserModule.h"
#include "Styling/SlateStyleRegistry.h"
#include "MixinGenerator.h"
#include "ReactorUtils.h"

#include "Brushes/SlateImageBrush.h"
#include "Styling/SlateStyle.h"

#define LOCTEXT_NAMESPACE "FAutoMixinEditorModule"

static const FString PUERTS_RESOURCES_PATH = FReactorUtils::GetPluginDir() / TEXT("Resources");

TSharedPtr<FSlateStyleSet> FAutoMixinEditorModule::StyleSet = nullptr;

// Stores the last foreground tab
static TWeakPtr<SDockTab> LastForegroundTab = nullptr;

// Tab switch event delegate handle
static FDelegateHandle TabForegroundedHandle;

// Editor subsystem reference
static UAssetEditorSubsystem* AssetEditorSubsystem = nullptr;

// Editor window instance
static IAssetEditorInstance* AssetEditorInstance = nullptr;

// Last active blueprint pointer
static UBlueprint* LastBlueprint = nullptr;

void FAutoMixinEditorModule::StartupModule()
{
	AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();

	FMixinGenerator::InitMixinTSFile();
	InitStyleSet();
	RegistrationButton();
	RegistrationContextButton();

	// Subscribe to tab foreground change events
	TabForegroundedHandle = FGlobalTabmanager::Get()->OnTabForegrounded_Subscribe(
		FOnActiveTabChanged::FDelegate::CreateLambda([](const TSharedPtr<SDockTab>& NewlyActiveTab, const TSharedPtr<SDockTab>& PreviouslyActiveTab)
		{
			if (!NewlyActiveTab.IsValid() || NewlyActiveTab == LastForegroundTab.Pin())
			{
				return;
			}
			// Skip tabs that aren't MajorTabs (e.g. EventGraph sub-tabs)
			if (NewlyActiveTab.Get()->GetTabRole() != MajorTab) return;
			LastForegroundTab = NewlyActiveTab;
			if (LastForegroundTab.IsValid())
			{
				// UE_LOG(LogTemp, Log, TEXT("Tab switched: %s"), *LastForegroundTab.Pin().Get()->GetTabLabel().ToString());
			}
		})
	);
}

void FAutoMixinEditorModule::RegistrationButton() const
{
	FBlueprintEditorModule& BlueprintEditorModule = FModuleManager::LoadModuleChecked<FBlueprintEditorModule>("Kismet");

	const TSharedPtr<FExtender> MenuExtender = MakeShareable(new FExtender());

	// Extend the toolbar in the Debugging section, inserted at the front
	MenuExtender->AddToolBarExtension(
		"Debugging",
		EExtensionHook::First,
		nullptr,
		FToolBarExtensionDelegate::CreateLambda([this](FToolBarBuilder& ToolbarBuilder)
		{
			ToolbarBuilder.AddToolBarButton(
				FUIAction(
					FExecuteAction::CreateLambda([this]()
					{
						ButtonPressed();
					})
				),
				NAME_None,
				LOCTEXT("GenerateTemplate", "Create TS File"),
				LOCTEXT("GenerateTemplateTooltip", "Generate TypeScript File"),
				FSlateIcon(GetStyleName(), "MixinIcon")
			);
		})
	);

	BlueprintEditorModule.GetMenuExtensibilityManager()->AddExtender(MenuExtender);
}

// Register a context menu entry in the Content Browser for selected assets
void FAutoMixinEditorModule::RegistrationContextButton() const
{
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
	TArray<FContentBrowserMenuExtender_SelectedAssets>& CBMenuAssetExtenderDelegates = ContentBrowserModule.GetAllAssetViewContextMenuExtenders();

	FContentBrowserMenuExtender_SelectedAssets MenuExtenderDelegate;
	MenuExtenderDelegate.BindLambda([this](const TArray<FAssetData>& SelectedAssets)
	{
		TSharedRef<FExtender> Extender = MakeShared<FExtender>();

		Extender->AddMenuExtension(
			"GetAssetActions",
			EExtensionHook::After,
			nullptr,
			FMenuExtensionDelegate::CreateLambda([this, SelectedAssets](FMenuBuilder& MenuBuilder)
			{
				MenuBuilder.AddMenuEntry(
					LOCTEXT("GenerateTSFile", "Create TS File"),
					LOCTEXT("GenerateTSFileTooltip", "Generate TypeScript File"),
					FSlateIcon(GetStyleName(), "MixinIcon"),
					FUIAction(
						FExecuteAction::CreateLambda([this, SelectedAssets]()
						{
							ContextButtonPressed(SelectedAssets);
						}),
						FCanExecuteAction::CreateLambda([SelectedAssets]()
						{
							return SelectedAssets.Num() > 0;
						})
					)
				);
			})
		);
		return Extender;
	});

	CBMenuAssetExtenderDelegates.Add(MenuExtenderDelegate);
}


void FAutoMixinEditorModule::ButtonPressed()
{
	// UE_LOG(LogTemp, Log, TEXT("ButtonPressed"));
	FMixinGenerator::GenerateBPMixinFile(GetActiveBlueprint());
}

void FAutoMixinEditorModule::ContextButtonPressed(const TArray<FAssetData>& SelectedAssets)
{
	if (SelectedAssets.IsEmpty()) return;
	for (const FAssetData& AssetData : SelectedAssets)
	{
		if (const UBlueprint* Blueprint = Cast<UBlueprint>(AssetData.GetAsset()))
		{
			FMixinGenerator::GenerateBPMixinFile(Blueprint);
		}
	}
}

/**
 * @brief Retrieves the currently active Blueprint.
 *
 * Iterates over all edited assets and finds the one whose editor tab
 * matches the last foregrounded tab.
 *
 * @return The active Blueprint, or nullptr if none was found.
 */
UBlueprint* FAutoMixinEditorModule::GetActiveBlueprint()
{
	const TArray<UObject*> EditedAssets = AssetEditorSubsystem->GetAllEditedAssets(); 
	for (UObject* EditedAsset : EditedAssets)
	{
		AssetEditorInstance = AssetEditorSubsystem->FindEditorForAsset(EditedAsset, false);

		if (!AssetEditorInstance || !IsValid(EditedAsset) || !EditedAsset->IsA<UBlueprint>()) continue;

		if (
			LastForegroundTab.Pin().Get()->GetTabLabel().ToString() ==
			AssetEditorInstance->GetAssociatedTabManager().Get()->GetOwnerTab().Get()->GetTabLabel().ToString()
		)
		{
			LastBlueprint = CastChecked<UBlueprint>(EditedAsset);
			break;
		}
	}

	return LastBlueprint;
}

FName FAutoMixinEditorModule::GetStyleName()
{
	return FName(TEXT("MixinEditorStyle"));
}

/**
 * @brief Initialises (or re-initialises) the Slate style set.
 *
 * Unregisters the existing style set if present, then creates a fresh
 * one containing the MixinIcon brush and registers it with Slate.
 */
void FAutoMixinEditorModule::InitStyleSet()
{
	// Unregister and reset if the style set already exists
	if (StyleSet.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSet);
		StyleSet.Reset();
	}

	StyleSet = MakeShared<FSlateStyleSet>(GetStyleName());

	const FString IconPath = PUERTS_RESOURCES_PATH / TEXT("CreateFilecon.png");
	StyleSet->Set("MixinIcon", new FSlateImageBrush(IconPath, FVector2D(40, 40)));

	// Register the new style set with the Slate style registry
	FSlateStyleRegistry::RegisterSlateStyle(*StyleSet);
}


void FAutoMixinEditorModule::ShutdownModule()
{
	if (StyleSet.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSet);
		StyleSet.Reset();
	}

	// Unsubscribe from the tab foreground change event
	FGlobalTabmanager::Get()->OnTabForegrounded_Unsubscribe(TabForegroundedHandle);
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAutoMixinEditorModule, AutoMixinEditor)
