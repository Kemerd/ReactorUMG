#include "UMGManager.h"
#include "ReactorUMGVersion.h"

#include "HttpModule.h"
#include "Components/PanelSlot.h"
#include "LogReactorUMG.h"
#include "ReactorUtils.h"
#include "Engine/Engine.h"
#include "Async/Async.h"
#include "Engine/AssetManager.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Engine/Font.h"
#include "Engine/StreamableManager.h"
#include "Interfaces/IHttpResponse.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Widget.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if defined(RIVE_SUPPORT) && RIVE_SUPPORT
#include "IRiveRendererModule.h"
#include "Rive/RiveFile.h"
#endif

/* -----------------------------------------------------------------------
 *  Static storage for batched property synchronization queues.
 *  Populated by QueueWidgetForSync / QueueSlotForSync, drained by
 *  FlushBatchedSync each frame.
 * ----------------------------------------------------------------------- */
TSet<TWeakObjectPtr<UWidget>>    UUMGManager::PendingWidgetSyncs;
TSet<TWeakObjectPtr<UPanelSlot>> UUMGManager::PendingSlotSyncs;

// ===================================================================
//  Widget creation
// ===================================================================

UReactorUIWidget* UUMGManager::CreateReactWidget(UWorld* World)
{
    return ::CreateWidget<UReactorUIWidget>(World);
}

UUserWidget* UUMGManager::CreateWidget(UWidgetTree* Outer, UClass* Class)
{
    return ::CreateWidget<UUserWidget>(Outer, Class);
}

// ===================================================================
//  Immediate property synchronization
// ===================================================================

void UUMGManager::SynchronizeWidgetProperties(UWidget* Widget)
{
#if REACTORUMG_HAS_WIDGET_SYNC_PROPERTIES
    if (Widget)
    {
        Widget->SynchronizeProperties();
    }
#endif
}

void UUMGManager::SynchronizeSlotProperties(UPanelSlot* Slot)
{
    if (Slot)
    {
        Slot->SynchronizeProperties();
    }
}

// ===================================================================
//  Batched property synchronization
// ===================================================================

void UUMGManager::QueueWidgetForSync(UWidget* Widget)
{
    if (Widget)
    {
        PendingWidgetSyncs.Add(Widget);
    }
}

void UUMGManager::QueueSlotForSync(UPanelSlot* Slot)
{
    if (Slot)
    {
        PendingSlotSyncs.Add(Slot);
    }
}

void UUMGManager::FlushBatchedSync()
{
#if REACTORUMG_HAS_WIDGET_SYNC_PROPERTIES
    /* Drain the widget queue -- stale weak pointers are silently skipped */
    for (const TWeakObjectPtr<UWidget>& WeakWidget : PendingWidgetSyncs)
    {
        if (UWidget* Widget = WeakWidget.Get())
        {
            Widget->SynchronizeProperties();
        }
    }
#endif

    /* Drain the slot queue */
    for (const TWeakObjectPtr<UPanelSlot>& WeakSlot : PendingSlotSyncs)
    {
        if (UPanelSlot* Slot = WeakSlot.Get())
        {
            Slot->SynchronizeProperties();
        }
    }

    PendingWidgetSyncs.Reset();
    PendingSlotSyncs.Reset();
}

// ===================================================================
//  Spine asset loading
//
//  Declarations are always present (UHT requirement), but the
//  implementations are no-ops when SpinePlugin is absent.
// ===================================================================

UObject* UUMGManager::LoadSpineAtlas(UObject* Context, const FString& AtlasPath, const FString& DirName)
{
#if WITH_SPINE_PLUGIN
    FString RawData;
    const FString AssetFilePath = ProcessAssetFilePath(AtlasPath, DirName);
    if (!FFileHelper::LoadFileToString(RawData, *AssetFilePath))
    {
        UE_LOG(LogReactorUMG, Error, TEXT("Spine atlas asset file( %s ) not exists."), *AssetFilePath);
        return nullptr;
    }
    
    USpineAtlasAsset* SpineAtlasAsset = NewObject<USpineAtlasAsset>(Context, USpineAtlasAsset::StaticClass(),
        NAME_None, RF_Public|RF_Transient);
    SpineAtlasAsset->SetRawData(RawData);
#if WITH_EDITORONLY_DATA
    SpineAtlasAsset->SetAtlasFileName(FName(*AssetFilePath));
#endif
    const FString BaseFilePath = FPaths::GetPath(AssetFilePath);

    /* Load all texture pages referenced by the atlas */
    spine::Atlas* Atlas = SpineAtlasAsset->GetAtlas();
    SpineAtlasAsset->atlasPages.Empty();

    spine::Vector<spine::AtlasPage*>& Pages = Atlas->getPages();
    for (size_t i = 0; i < Pages.size(); ++i)
    {
        spine::AtlasPage* P = Pages[i];
        const FString SourceTextureFilename = FPaths::Combine(*BaseFilePath, UTF8_TO_TCHAR(P->name.buffer()));
        UTexture2D* Texture = UKismetRenderingLibrary::ImportFileAsTexture2D(SpineAtlasAsset, SourceTextureFilename);
        SpineAtlasAsset->atlasPages.Add(Texture); 
    }
    
    return SpineAtlasAsset;
#else
    UE_LOG(LogReactorUMG, Warning, TEXT("LoadSpineAtlas called but SpinePlugin is not available"));
    return nullptr;
#endif // WITH_SPINE_PLUGIN
}

UObject* UUMGManager::LoadSpineSkeleton(UObject* Context, const FString& SkeletonPath, const FString& DirName)
{
#if WITH_SPINE_PLUGIN
    TArray<uint8> RawData;
    const FString AssetFilePath = ProcessAssetFilePath(SkeletonPath, DirName);
    if (!FFileHelper::LoadFileToArray(RawData, *AssetFilePath, 0))
    {
        UE_LOG(LogReactorUMG, Error, TEXT("Spine skeleton asset file( %s ) not exists."), *AssetFilePath);
        return nullptr;
    }

    USpineSkeletonDataAsset* SkeletonDataAsset = NewObject<USpineSkeletonDataAsset>(Context,
            USpineSkeletonDataAsset::StaticClass(), NAME_None, RF_Transient | RF_Public);

#if WITH_EDITORONLY_DATA
    SkeletonDataAsset->SetSkeletonDataFileName(FName(*AssetFilePath));
#endif
    SkeletonDataAsset->SetRawData(RawData);

    return SkeletonDataAsset;
#else
    UE_LOG(LogReactorUMG, Warning, TEXT("LoadSpineSkeleton called but SpinePlugin is not available"));
    return nullptr;
#endif // WITH_SPINE_PLUGIN
}

// ===================================================================
//  Rive asset loading (only compiled when Rive support is present)
// ===================================================================

#if defined(RIVE_SUPPORT) && RIVE_SUPPORT
URiveFile* UUMGManager::LoadRiveFile(UObject* Context, const FString& RivePath, const FString& DirName)
{
    if (!IRiveRendererModule::Get().GetRenderer())
    {
        UE_LOG(LogReactorUMG, Error,
            TEXT("Unable to import the Rive file '%s': the Renderer is null"), *RivePath);
        return nullptr;
    }

    const FString RiveAssetFilePath = ProcessAssetFilePath(RivePath, DirName);
    if (!FPaths::FileExists(RiveAssetFilePath))
    {
        UE_LOG(LogReactorUMG, Error,
            TEXT("Unable to import the Rive file '%s': the file does not exist"), *RiveAssetFilePath);
        return nullptr;
    }

    if (!Context)
    {
        UE_LOG(LogReactorUMG, Error,
            TEXT("Unable to create the Rive file '%s': the context is null"), *RiveAssetFilePath);
        return nullptr;
    }

    TArray<uint8> FileBuffer;
    if (!FFileHelper::LoadFileToArray(FileBuffer, *RiveAssetFilePath))
    {
        UE_LOG(LogReactorUMG, Error,
            TEXT("Unable to import the Rive file '%s': Could not read the file"), *RiveAssetFilePath);
        return nullptr;
    }
    
    URiveFile* RiveFile =
        NewObject<URiveFile>(Context, URiveFile::StaticClass(), NAME_None, RF_Transient | RF_Public);
    check(RiveFile);
    
#if WITH_EDITORONLY_DATA
    if (!RiveFile->EditorImport(RiveAssetFilePath, FileBuffer))
    {
        UE_LOG(LogReactorUMG, Error,
            TEXT("Failed to import the Rive file '%s': Could not import the riv file"), *RiveAssetFilePath);
        RiveFile->ConditionalBeginDestroy();
        return nullptr;
    }
#endif
    
    return RiveFile;
}
#endif // RIVE_SUPPORT

UWorld* UUMGManager::GetCurrentWorld()
{
    if (GEngine && GEngine->GetWorld())
    {
        return GEngine->GetWorld();
    }
    
    return nullptr;
}

UObject* UUMGManager::FindFontFamily(const TArray<FString>& Names, UObject* InOuter)
{
    const FString FontDir = TEXT("/ReactorUMG/FontFamily");
    for (const FString& Name : Names)
    {
        const FString FontAssetPath = FontDir / Name + TEXT(".") + Name;
        if (UFont* Font = Cast<UFont>(StaticLoadObject(UFont::StaticClass(), InOuter, *FontAssetPath)))
        {
            return Font;
        }
    }

    const FString DefaultEngineFont = TEXT("/Engine/EngineFonts/Roboto.Roboto");
    if (UFont* DefaultFont = Cast<UFont>(StaticLoadObject(UFont::StaticClass(), InOuter, *DefaultEngineFont)))
    {
        return DefaultFont;
    }
    
    return nullptr;
}

FVector2D UUMGManager::GetWidgetGeometrySize(UWidget* Widget)
{
    if (!Widget)
    {
        return FVector2D::ZeroVector;
    }
    
    const FGeometry Geometry = Widget->GetPaintSpaceGeometry();
    const auto Size = Geometry.GetAbsoluteSize();
    FVector2D Result;
    Result.X = Size.X;
    Result.Y = Size.Y;
    return Result;
}

void UUMGManager::LoadBrushImageObject(const FString& ImagePath, FAssetLoadedDelegate OnLoaded,
    FEasyDelegate OnFailed, UObject* Context, bool bIsSyncLoad, const FString& DirName)
{
    if (ImagePath.StartsWith(TEXT("/")))
    {
        // Handle UE asset package paths
        LoadImageBrushAsset(ImagePath, Context, bIsSyncLoad, OnLoaded, OnFailed);
        return;
    }

    if (ImagePath.StartsWith(TEXT("http")) || ImagePath.StartsWith(TEXT("HTTP")))
    {
        // Handle network resources
        LoadImageTextureFromURL(ImagePath, Context, bIsSyncLoad, OnLoaded, OnFailed);
        return;
    }

    const FString AbsPath = ProcessAssetFilePath(ImagePath, DirName);
    if (!FPaths::FileExists(*AbsPath))
    {
        UE_LOG(LogReactorUMG, Error, TEXT("Image file( %s ) not exists."), *AbsPath);
        return; 
    }
    
    LoadImageTextureFromLocalFile(AbsPath, Context, bIsSyncLoad, OnLoaded, OnFailed);
}

FString UUMGManager::GetAbsoluteJSContentPath(const FString& RelativePath, const FString& DirName)
{
    if (DirName.IsEmpty() || RelativePath.IsEmpty())
    {
        return RelativePath;
    }

    FString AbsolutePath = RelativePath;
    if (FPaths::IsRelative(RelativePath))
    {
        // Handle relative path case
        AbsolutePath = FPaths::ConvertRelativePathToFull(DirName / RelativePath);
    }
    
    return AbsolutePath;
}

FString UUMGManager::ProcessAssetFilePath(const FString& RelativePath, const FString& DirName)
{
    if (!FPaths::FileExists(*RelativePath))
    {
        FString AbsPath = GetAbsoluteJSContentPath(RelativePath, DirName);
        if (!FPaths::FileExists(*AbsPath))
        {
            AbsPath = FReactorUtils::ConvertRelativePathToFullUsingTSConfig(RelativePath, DirName);
        }

        return AbsPath;
    }
    
    return RelativePath;
}

void UUMGManager::LoadImageBrushAsset(const FString& AssetPath, UObject* Context,
    bool bIsSyncLoad, FAssetLoadedDelegate OnLoaded, FEasyDelegate OnFailed)
{
    FSoftObjectPath SoftObjectPath(AssetPath);
    if (bIsSyncLoad)
    {
        UAssetManager::GetStreamableManager().RequestSyncLoad(SoftObjectPath);
        UObject* LoadedObject = SoftObjectPath.ResolveObject();
        if (LoadedObject)
        {
            LoadedObject->AddToCluster(Context);
            OnLoaded.ExecuteIfBound(LoadedObject);
        } else
        {
            OnFailed.ExecuteIfBound();
        }
    }else
    {
        UAssetManager::GetStreamableManager().RequestAsyncLoad(SoftObjectPath,
            FStreamableDelegate::CreateLambda([SoftObjectPath, OnLoaded, OnFailed]()
        {
                UObject* LoadedObject = SoftObjectPath.ResolveObject();
                if (LoadedObject)
                {
                    OnLoaded.ExecuteIfBound(LoadedObject);
                } else
                {
                    OnFailed.ExecuteIfBound();
                }
        }));   
    }
}

void UUMGManager::LoadImageTextureFromLocalFile(const FString& FilePath, UObject* Context,
    bool bIsSyncLoad, FAssetLoadedDelegate OnLoaded, FEasyDelegate OnFailed)
{
    if (bIsSyncLoad)
    {
        UTexture2D* Texture = UKismetRenderingLibrary::ImportFileAsTexture2D(nullptr, FilePath);
        if (Texture)
        {
            Texture->AddToCluster(Context);
            OnLoaded.ExecuteIfBound(Texture);
        } else
        {
            OnFailed.ExecuteIfBound();
        }
    }else
    {
        AsyncTask(ENamedThreads::GameThread, [FilePath, Context, OnLoaded, OnFailed]()
        {
            UTexture2D *Texture = UKismetRenderingLibrary::ImportFileAsTexture2D(nullptr, FilePath);
            if (Texture)
            {
                Texture->AddToCluster(Context);
                OnLoaded.ExecuteIfBound(Texture);
            } else
            {
                OnFailed.ExecuteIfBound();
            }
        });
    }
}

void UUMGManager::LoadImageTextureFromURL(const FString& Url, UObject* Context,
    bool bIsSyncLoad, FAssetLoadedDelegate OnLoaded, FEasyDelegate OnFailed)
{
    FHttpModule& HTTP = FHttpModule::Get();
    TSharedRef<IHttpRequest> HttpRequest = HTTP.CreateRequest();
    HttpRequest->OnProcessRequestComplete().BindLambda(
        [Url, Context, OnLoaded, OnFailed](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
        {
            check(IsInGameThread());
            if (!bWasSuccessful || !Response.IsValid())
            {
                UE_LOG(LogTemp, Error, TEXT("Failed to download image."));
                OnFailed.ExecuteIfBound();
                return;
            }

            TArray<uint8> ImageData = Response->GetContent();
            UTexture2D* Texture = UKismetRenderingLibrary::ImportBufferAsTexture2D(nullptr, ImageData);
            if (Texture)
            {
                UE_LOG(LogReactorUMG, Log, TEXT("Download image file successfully for url: %s"), *Url);
                Texture->AddToCluster(Context);
                OnLoaded.ExecuteIfBound(Texture);
            } else
            {
                OnFailed.ExecuteIfBound();
            }
        });
    
    HttpRequest->SetURL(Url);
    HttpRequest->SetVerb("GET");
    HttpRequest->ProcessRequest();
}

void UUMGManager::AddRootWidgetToWidgetTree(UWidgetTree* Container, UWidget* RootWidget)
{
    if (Container == nullptr || RootWidget == nullptr)
    {
        return;
    }

    if (!Container->IsA(UWidgetTree::StaticClass()))
    {
        return;
    }

    RootWidget->RemoveFromParent();

    EObjectFlags NewObjectFlags = RF_Transactional;
    if (Container->HasAnyFlags(RF_Transient))
    {
        NewObjectFlags |= RF_Transient;
    }

    UPanelSlot* PanelSlot = NewObject<UPanelSlot>(Container, UPanelSlot::StaticClass(), NAME_None, NewObjectFlags);
    PanelSlot->Content = RootWidget;
	
    RootWidget->Slot = PanelSlot;
	
    if (Container)
    {
        Container->RootWidget = RootWidget;
    }
}

void UUMGManager::RemoveRootWidgetFromWidgetTree(UWidgetTree* Container, UWidget* RootWidget)
{
    if (RootWidget == nullptr || Container == nullptr)
    {
        return;
    }
    
    if (!Container->IsA(UWidgetTree::StaticClass()))
    {
        return;
    }
    
    UPanelSlot* PanelSlot = RootWidget->Slot;
    if (PanelSlot == nullptr)
    {
        return;
    }

    if (PanelSlot->Content)
    {
        PanelSlot->Content->Slot = nullptr;
    }

    PanelSlot->ReleaseSlateResources(true);
    PanelSlot->Parent = nullptr;
    PanelSlot->Content = nullptr;

    Container->RootWidget = nullptr;
}

FVector2D UUMGManager::GetWidgetScreenPixelSize(UWidget* Widget, bool bReturnInLogicalViewportUnits)
{
    if (!IsValid(Widget))
    {
        return FVector2D::ZeroVector;
    }

    // Note: On the same frame as first AddToViewport, geometry may not be laid out yet, size could be zero.
    const FGeometry& Geo = Widget->GetCachedGeometry();

    // Method A: Use layout bounding rect (absolute/desktop space)
    const FSlateRect Rect = Geo.GetLayoutBoundingRect();
    FVector2D SizePixels(Rect.Right - Rect.Left, Rect.Bottom - Rect.Top);

    //（可选）如果你想要“逻辑单位”（DPI 缩放前的 viewport 单位），除以当前 ViewportScale
    if (bReturnInLogicalViewportUnits)
    {
        const float Scale = UWidgetLayoutLibrary::GetViewportScale(Widget);
        if (Scale > 0.f)
        {
            SizePixels /= Scale;
        }
    }

    return SizePixels;
}

FVector2D UUMGManager::GetCanvasSizeDIP(UObject* WorldContextObject)
{
    const FVector2D ViewportSizePx = UWidgetLayoutLibrary::GetViewportSize(WorldContextObject);
    const float Scale = UWidgetLayoutLibrary::GetViewportScale(WorldContextObject);
    return ViewportSizePx / FMath::Max(Scale, 0.0001f);
}

// ===================================================================
//  Runtime gradient texture generation
//
//  Generates transient UTexture2D objects filled with gradients via
//  direct pixel writes. The textures are BGRA8 format for maximum
//  Slate/UMG compatibility. They are RF_Transient so they are never
//  serialized or saved to disk.
// ===================================================================

/**
 * Helper: interpolates between color stops at position t (0..1).
 * Assumes Positions is sorted ascending and Colors.Num() == Positions.Num().
 */
static FLinearColor SampleGradient(
    const TArray<FLinearColor>& Colors,
    const TArray<float>& Positions,
    float t)
{
    if (Colors.Num() == 0)
    {
        return FLinearColor::Black;
    }
    if (Colors.Num() == 1 || t <= Positions[0])
    {
        return Colors[0];
    }
    if (t >= Positions.Last())
    {
        return Colors.Last();
    }

    /* Find the two surrounding stops */
    for (int32 i = 0; i < Positions.Num() - 1; ++i)
    {
        if (t >= Positions[i] && t <= Positions[i + 1])
        {
            const float Range = Positions[i + 1] - Positions[i];
            const float LocalT = (Range > SMALL_NUMBER) ? (t - Positions[i]) / Range : 0.f;
            return FMath::Lerp(Colors[i], Colors[i + 1], LocalT);
        }
    }

    return Colors.Last();
}

UTexture2D* UUMGManager::CreateLinearGradientTexture(
    UObject* Context,
    float AngleDegrees,
    const TArray<FLinearColor>& Colors,
    const TArray<float>& Positions,
    int32 Width,
    int32 Height)
{
    if (Colors.Num() == 0 || Colors.Num() != Positions.Num())
    {
        UE_LOG(LogReactorUMG, Warning, TEXT("CreateLinearGradientTexture: Colors/Positions mismatch or empty"));
        return nullptr;
    }

    Width  = FMath::Clamp(Width,  1, 2048);
    Height = FMath::Clamp(Height, 1, 2048);

    /* Create a transient texture in BGRA8 format */
    UTexture2D* Texture = UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8);
    if (!Texture)
    {
        return nullptr;
    }

    Texture->MipGenSettings     = TMGS_NoMipmaps;
    Texture->CompressionSettings = TC_EditorIcon;
    Texture->SRGB               = true;
    Texture->Filter             = TF_Bilinear;
    Texture->AddressX           = TA_Clamp;
    Texture->AddressY           = TA_Clamp;

    /* CSS gradient angle: 0deg = bottom-to-top, 90deg = left-to-right.
     * Convert to a direction vector. */
    const float AngleRad = FMath::DegreesToRadians(AngleDegrees);
    const float DirX = FMath::Sin(AngleRad);
    const float DirY = -FMath::Cos(AngleRad); // negative because pixel Y goes downward

    /* Lock mip 0 and fill pixels */
    FTexture2DMipMap& Mip = Texture->GetPlatformData()->Mips[0];
    void* RawData = Mip.BulkData.Lock(LOCK_READ_WRITE);
    uint8* Pixels = static_cast<uint8*>(RawData);

    for (int32 Y = 0; Y < Height; ++Y)
    {
        for (int32 X = 0; X < Width; ++X)
        {
            /* Normalized coordinates 0..1 */
            const float NX = (Width  > 1) ? static_cast<float>(X) / (Width  - 1) : 0.5f;
            const float NY = (Height > 1) ? static_cast<float>(Y) / (Height - 1) : 0.5f;

            /* Project onto the gradient direction line.
             * The dot product of (NX-0.5, NY-0.5) and direction gives -0.5..0.5,
             * so we add 0.5 to normalize to 0..1. */
            const float Proj = (NX - 0.5f) * DirX + (NY - 0.5f) * DirY + 0.5f;
            const float T = FMath::Clamp(Proj, 0.f, 1.f);

            const FLinearColor Sampled = SampleGradient(Colors, Positions, T);
            const FColor SRGB = Sampled.ToFColor(true);

            const int32 Idx = (Y * Width + X) * 4;
            Pixels[Idx + 0] = SRGB.B;
            Pixels[Idx + 1] = SRGB.G;
            Pixels[Idx + 2] = SRGB.R;
            Pixels[Idx + 3] = SRGB.A;
        }
    }

    Mip.BulkData.Unlock();
    Texture->UpdateResource();

    return Texture;
}

UTexture2D* UUMGManager::CreateRadialGradientTexture(
    UObject* Context,
    const TArray<FLinearColor>& Colors,
    const TArray<float>& Positions,
    int32 Width,
    int32 Height)
{
    if (Colors.Num() == 0 || Colors.Num() != Positions.Num())
    {
        UE_LOG(LogReactorUMG, Warning, TEXT("CreateRadialGradientTexture: Colors/Positions mismatch or empty"));
        return nullptr;
    }

    Width  = FMath::Clamp(Width,  1, 2048);
    Height = FMath::Clamp(Height, 1, 2048);

    UTexture2D* Texture = UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8);
    if (!Texture)
    {
        return nullptr;
    }

    Texture->MipGenSettings     = TMGS_NoMipmaps;
    Texture->CompressionSettings = TC_EditorIcon;
    Texture->SRGB               = true;
    Texture->Filter             = TF_Bilinear;
    Texture->AddressX           = TA_Clamp;
    Texture->AddressY           = TA_Clamp;

    FTexture2DMipMap& Mip = Texture->GetPlatformData()->Mips[0];
    void* RawData = Mip.BulkData.Lock(LOCK_READ_WRITE);
    uint8* Pixels = static_cast<uint8*>(RawData);

    /* Radial gradient: distance from center (0..1) drives the color lookup */
    const float CenterX = 0.5f;
    const float CenterY = 0.5f;

    for (int32 Y = 0; Y < Height; ++Y)
    {
        for (int32 X = 0; X < Width; ++X)
        {
            const float NX = (Width  > 1) ? static_cast<float>(X) / (Width  - 1) : 0.5f;
            const float NY = (Height > 1) ? static_cast<float>(Y) / (Height - 1) : 0.5f;

            const float DX = NX - CenterX;
            const float DY = NY - CenterY;
            /* Distance from center, scaled so corners are at ~1.0.
             * Using *2 so the edge of the inscribed circle is at 1.0. */
            const float Dist = FMath::Clamp(FMath::Sqrt(DX * DX + DY * DY) * 2.f, 0.f, 1.f);

            const FLinearColor Sampled = SampleGradient(Colors, Positions, Dist);
            const FColor SRGB = Sampled.ToFColor(true);

            const int32 Idx = (Y * Width + X) * 4;
            Pixels[Idx + 0] = SRGB.B;
            Pixels[Idx + 1] = SRGB.G;
            Pixels[Idx + 2] = SRGB.R;
            Pixels[Idx + 3] = SRGB.A;
        }
    }

    Mip.BulkData.Unlock();
    Texture->UpdateResource();

    return Texture;
}
