/**
 * @file UMGManager.h
 * @brief Core Blueprint function library for ReactorUMG widget management.
 *
 * Provides static utility functions exposed to Blueprint and TypeScript for
 * creating widgets, synchronizing properties, loading assets (images, fonts,
 * Spine skeletons, Rive files), and querying viewport geometry. All heavy
 * asset I/O supports both synchronous and asynchronous paths to avoid
 * blocking the game thread when possible.
 */

#pragma once

#include "CoreMinimal.h"
#include "ReactorUIWidget.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameFramework/PlayerController.h"

/* -----------------------------------------------------------------
 * Optional middleware plugin includes -- only pulled in when the
 * corresponding build flag is set (see ReactorUMG.Build.cs).
 * ----------------------------------------------------------------- */
#if WITH_SPINE_PLUGIN
#include "SpineSkeletonDataAsset.h"
#include "SpineAtlasAsset.h"
#endif // WITH_SPINE_PLUGIN

#if defined(RIVE_SUPPORT) && RIVE_SUPPORT
#include "Rive/RiveDescriptor.h"
#endif // RIVE_SUPPORT

#include "UMGManager.generated.h"

/** Simple parameterless dynamic delegate used for failure callbacks. */
DECLARE_DYNAMIC_DELEGATE(FEasyDelegate);

/** Dynamic delegate fired when an asset finishes loading. */
DECLARE_DYNAMIC_DELEGATE_OneParam(FAssetLoadedDelegate, UObject*, Object);

/**
 * @class UUMGManager
 * @brief Static Blueprint function library that bridges TypeScript/React
 *        and the Unreal widget system.
 *
 * Every function here is intended to be called from PuerTS-hosted JavaScript
 * or directly from Blueprints. The class is stateless; all state lives in the
 * widgets and environments themselves.
 */
UCLASS(BlueprintType)
class REACTORUMG_API UUMGManager : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:

    // ---------------------------------------------------------------
    //  Widget creation
    // ---------------------------------------------------------------

    /**
     * @brief Create a new ReactorUIWidget owned by the given world.
     * @param World  The owning UWorld.
     * @return       A freshly constructed UReactorUIWidget.
     */
    UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "Widget|ReactorUMG")
    static UReactorUIWidget* CreateReactWidget(UWorld* World);

    /**
     * @brief Create a generic UUserWidget of an arbitrary class inside a WidgetTree.
     * @param Outer  The WidgetTree that will own the widget.
     * @param Class  UClass of the widget to instantiate.
     * @return       The constructed UUserWidget (or nullptr on failure).
     */
    UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "Widget|ReactorUMG")
    static UUserWidget* CreateWidget(UWidgetTree* Outer, UClass* Class);

    // ---------------------------------------------------------------
    //  Property synchronization
    // ---------------------------------------------------------------

    /**
     * @brief Force a single widget to synchronize its visual properties.
     *
     * Calls UWidget::SynchronizeProperties() which pushes all UPROPERTY
     * changes into the underlying Slate widget. On UE 5.2 and earlier
     * this is a no-op as the API was not yet available.
     *
     * @param Widget  The widget to synchronize.
     */
    UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "Widget|ReactorUMG")
    static void SynchronizeWidgetProperties(UWidget* Widget);

    /**
     * @brief Force a panel slot to synchronize its layout properties.
     * @param Slot  The slot to synchronize.
     */
    UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "Widget|ReactorUMG")
    static void SynchronizeSlotProperties(UPanelSlot* Slot);

    /**
     * @brief Queue a widget for batched property synchronization.
     *
     * Instead of immediately calling SynchronizeProperties(), this adds
     * the widget to an internal set that will be flushed once per frame
     * via FlushBatchedSync(). This dramatically reduces redundant Slate
     * rebuilds when the React reconciler touches many widgets in a single
     * commit.
     *
     * @param Widget  The widget whose properties need syncing.
     */
    UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "Widget|ReactorUMG")
    static void QueueWidgetForSync(UWidget* Widget);

    /**
     * @brief Queue a panel slot for batched layout synchronization.
     * @param Slot  The slot whose layout needs syncing.
     */
    UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "Widget|ReactorUMG")
    static void QueueSlotForSync(UPanelSlot* Slot);

    /**
     * @brief Flush all pending batched widget/slot property syncs.
     *
     * Should be called once per frame after the React reconciler has
     * finished its commit phase. Drains both the widget and slot queues.
     */
    UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "Widget|ReactorUMG")
    static void FlushBatchedSync();

    // ---------------------------------------------------------------
    //  Spine asset loading
    //
    //  UHT forbids UFUNCTION inside arbitrary #if blocks, so these
    //  are always declared. When WITH_SPINE_PLUGIN == 0 the
    //  implementations simply return nullptr.
    // ---------------------------------------------------------------

    /**
     * @brief Load a Spine skeleton data asset from disk.
     *
     * Returns nullptr when the SpinePlugin module is not present.
     *
     * @param Context       Outer object for lifecycle management.
     * @param SkeletonPath  Relative or absolute path to the skeleton file.
     * @param DirName       Base directory for relative path resolution.
     * @return              The loaded asset as UObject*, or nullptr.
     */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "Widget|Spine")
	static UObject* LoadSpineSkeleton(UObject* Context, const FString& SkeletonPath, const FString& DirName);

    /**
     * @brief Load a Spine atlas asset (texture pages included) from disk.
     *
     * Returns nullptr when the SpinePlugin module is not present.
     *
     * @param Context    Outer object for lifecycle management.
     * @param AtlasPath  Relative or absolute path to the atlas file.
     * @param DirName    Base directory for relative path resolution.
     * @return           The loaded asset as UObject*, or nullptr.
     */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category="Widget|Spine")
	static UObject* LoadSpineAtlas(UObject* Context, const FString& AtlasPath, const FString& DirName);

    // ---------------------------------------------------------------
    //  World / viewport queries
    // ---------------------------------------------------------------

    /**
     * @brief Retrieve the current UWorld from GEngine.
     * @return The active world, or nullptr if the engine is not ready.
     */
	UFUNCTION(BlueprintCallable, Category="Widget|ReactorUMG")
	static UWorld* GetCurrentWorld();

    /**
     * @brief Search plugin-bundled font assets by family name.
     *
     * Iterates through @p Names in order, returning the first font
     * found under /ReactorUMG/FontFamily/. Falls back to the engine
     * default Roboto if none match.
     *
     * @param Names    Ordered list of font family names to search.
     * @param InOuter  Parent object for lifecycle management.
     * @return         The UFont object, or nullptr if nothing was found.
     */
    UFUNCTION(BlueprintCallable, Category="Widget|ReactorUMG")
	static UObject* FindFontFamily(const TArray<FString>& Names, UObject* InOuter);

    /**
     * @brief Get the absolute paint-space geometry size of a widget.
     * @param Widget  The target widget.
     * @return        Width/height in absolute pixels.
     */
	UFUNCTION(BlueprintCallable, Category="Widget|ReactorUMG")
	static FVector2D GetWidgetGeometrySize(UWidget* Widget);

    /**
     * @brief Load an image for use as a Slate brush from various sources.
     *
     * Automatically detects the source type:
     * - UE asset path (starts with "/")
     * - HTTP/HTTPS URL
     * - Local filesystem path (resolved via DirName)
     *
     * @param ImagePath   The image path or URL.
     * @param OnLoaded    Delegate fired on successful load with the UObject.
     * @param OnFailed    Delegate fired on failure.
     * @param Context     Outer object for lifecycle clustering.
     * @param bIsSyncLoad If true, blocks until the image is ready.
     * @param DirName     Base directory for relative path resolution.
     */
	UFUNCTION(BlueprintCallable, Category="Widget|ReactorUMG")
	static void LoadBrushImageObject(const FString& ImagePath,
		FAssetLoadedDelegate OnLoaded, FEasyDelegate OnFailed, UObject* Context = nullptr, bool bIsSyncLoad = true, const FString& DirName = TEXT(""));

    /**
     * @brief Resolve a relative JS content path to an absolute filesystem path.
     * @param RelativePath  The relative path from JS.
     * @param DirName       The base directory for resolution.
     * @return              The fully qualified absolute path.
     */
	UFUNCTION(BlueprintCallable, Category="Widget|ReactorUMG")
	static FString GetAbsoluteJSContentPath(const FString& RelativePath, const FString& DirName);

    /**
     * @brief Attach a root widget to a UWidgetTree container.
     * @param Container  The WidgetTree to receive the root widget.
     * @param Content    The widget to install as root.
     */
	UFUNCTION(BlueprintCallable, Category="Widget|ReactorUMG")
	static void AddRootWidgetToWidgetTree(UWidgetTree* Container, UWidget* Content);

    /**
     * @brief Detach a root widget from its UWidgetTree container.
     * @param Container  The WidgetTree currently holding the root.
     * @param Content    The widget to remove.
     */
	UFUNCTION(BlueprintCallable, Category="Widget|ReactorUMG")
	static void RemoveRootWidgetFromWidgetTree(UWidgetTree* Container, UWidget* Content);

    /**
     * @brief Get a widget's on-screen size in physical pixels.
     * @param Widget                        The target widget.
     * @param bReturnInLogicalViewportUnits  If true, divide by DPI scale.
     * @return                               Width/height in pixels.
     */
	UFUNCTION(BlueprintCallable, Category="Widget|ReactorUMG")
	static FVector2D GetWidgetScreenPixelSize(UWidget* Widget, bool bReturnInLogicalViewportUnits = false);

    /**
     * @brief Get the canvas (viewport) size in DPI-independent pixels.
     * @param WorldContextObject  Any object that can resolve a UWorld.
     * @return                    Canvas width/height in DIPs.
     */
	UFUNCTION(BlueprintCallable, Category="Widget|ReactorUMG")
	static FVector2D GetCanvasSizeDIP(UObject* WorldContextObject);

private:
	/** Resolve a relative asset path using DirName or tsconfig.json paths. */
	static FString ProcessAssetFilePath(const FString& RelativePath, const FString& DirName);

	/** Load a UE asset package path into a brush-compatible UObject. */
	static void LoadImageBrushAsset(const FString& AssetPath, UObject* Context, bool bIsSyncLoad, FAssetLoadedDelegate OnLoaded, FEasyDelegate OnFailed);

	/** Load a local image file into a UTexture2D. */
	static void LoadImageTextureFromLocalFile(const FString& FilePath, UObject* Context, bool bIsSyncLoad, FAssetLoadedDelegate OnLoaded, FEasyDelegate OnFailed);

	/** Download an image from a URL and create a UTexture2D. */
	static void LoadImageTextureFromURL(const FString& Url, UObject* Context, bool bIsSyncLoad, FAssetLoadedDelegate OnLoaded, FEasyDelegate OnFailed);

	// ---------------------------------------------------------------
	//  Batched synchronization internals
	// ---------------------------------------------------------------

	/** Set of widgets queued for SynchronizeProperties this frame. */
	static TSet<TWeakObjectPtr<UWidget>> PendingWidgetSyncs;

	/** Set of slots queued for SynchronizeProperties this frame. */
	static TSet<TWeakObjectPtr<UPanelSlot>> PendingSlotSyncs;
};
