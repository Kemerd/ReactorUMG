#include "ReactorUIWidget.h"
#include "JsBridgeCaller.h"
#include "JsEnvRuntime.h"
#include "LogReactorUMG.h"
#include "ReactorUMGBlueprintGeneratedClass.h"
#include "ReactorUtils.h"
#include "UMGManager.h"
#include "Blueprint/WidgetTree.h"

UReactorUIWidget::UReactorUIWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer), CustomJSArg(nullptr), LaunchScriptPath(TEXT("")),
		JsEnv(nullptr), bWidgetTreeInitialized(false)
{
}

bool UReactorUIWidget::Initialize()
{
	bool SuperRes = Super::Initialize();
#if WITH_EDITOR
	if (GIsPlayInEditorWorld)
	{
		SetNewWidgetTree();
	}
	return SuperRes;
#else
	SetNewWidgetTree();
	return SuperRes;
#endif
}

void UReactorUIWidget::BeginDestroy()
{
	/* -----------------------------------------------------------
	 * Tear down the JS bridge caller so the static map does not
	 * hold a dangling pointer to this widget's delegate, and
	 * release the JS environment back to the pool.
	 * ----------------------------------------------------------- */
	if (!LaunchScriptPath.IsEmpty())
	{
		UJsBridgeCaller::RemoveBridgeCaller(LaunchScriptPath);
		UE_LOG(LogReactorUMG, Log,
			TEXT("BeginDestroy: removed bridge caller for %s"), *LaunchScriptPath);
	}

	ReleaseJsEnv();

	/* Null out transient object pointers so GC can collect them */
	CustomJSArg = nullptr;

	Super::BeginDestroy();
}

void UReactorUIWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	/*
	 * Flush any batched widget/slot property synchronizations that the
	 * React reconciler queued during its commit phase. Doing this here
	 * means all Slate property pushes happen once per frame, in bulk,
	 * instead of N times per property mutation.
	 */
	UUMGManager::FlushBatchedSync();
}

void UReactorUIWidget::SetNewWidgetTree()
{
	if (!bWidgetTreeInitialized && !HasAnyFlags(RF_ClassDefaultObject))
	{
		if (WidgetTree != nullptr)
		{
			UWidgetTree* OldWidgetTree = WidgetTree;
			WidgetTree = NewObject<UWidgetTree>(this, NAME_None, RF_Transient);
			OldWidgetTree->MarkAsGarbage();
			OldWidgetTree = nullptr;
		}
		
		UReactorUMGBlueprintGeneratedClass* ReactorBPGC = Cast<UReactorUMGBlueprintGeneratedClass>(GetClass());
		if (ReactorBPGC)
		{
			TsProjectDir = ReactorBPGC->TsProjectDir;
			TsScriptHomeRelativeDir = ReactorBPGC->TsScriptHomeRelativeDir;
			LaunchScriptPath = ReactorBPGC->MainScriptPath;
			if (!LaunchScriptPath.IsEmpty())
			{
				RunScriptToInitWidgetTree();
			}
		}

		UE_LOG(LogReactorUMG, Log, TEXT("Setup reactor ui widget with script path: %s"), *LaunchScriptPath);
		
		bWidgetTreeInitialized = true;
	}
}


#if WITH_EDITOR
const FText UReactorUIWidget::GetPaletteCategory()
{
	return NSLOCTEXT("ReactorUMG", "UIWidget", "ReactorUMG");
}
#endif

void UReactorUIWidget::RunScriptToInitWidgetTree()
{
	if (!LaunchScriptPath.IsEmpty() && (!UJsBridgeCaller::IsExistBridgeCaller(LaunchScriptPath) || FReactorUtils::IsAnyPIERunning()))
	{
		TArray<TPair<FString, UObject*>> Arguments;
		UJsBridgeCaller* Caller = UJsBridgeCaller::AddNewBridgeCaller(LaunchScriptPath);
		Arguments.Add(TPair<FString, UObject*>(TEXT("ReactorUIWidget_BridgeCaller"), Caller));
		Arguments.Add(TPair<FString, UObject*>(TEXT("WidgetTree"), this->WidgetTree));

		if (!CustomJSArg)
		{
			CustomJSArg = NewObject<UCustomJSArg>(this, FName("ReactorUIWidget_CustomArgs"), RF_Transient);
		}
		CustomJSArg->bIsUsingBridgeCaller = true;
		Arguments.Add(TPair<FString, UObject*>(TEXT("CustomArgs"), CustomJSArg));
		
		JsEnv = FJsEnvRuntime::GetInstance().GetFreeJsEnv();
		if (JsEnv)
		{
			if (!FReactorUtils::IsAnyPIERunning())
			{
				const bool Result = FJsEnvRuntime::GetInstance().StartJavaScript(JsEnv, LaunchScriptPath, Arguments);
				if (!Result)
				{
					UJsBridgeCaller::RemoveBridgeCaller(LaunchScriptPath);
					ReleaseJsEnv();
					UE_LOG(LogReactorUMG, Warning, TEXT("Start ui javascript file %s failed"), *LaunchScriptPath);
				}		
			} else
			{
				const FString JSScriptContentDir = FReactorUtils::GetTSCBuildOutDirFromTSConfig(TsProjectDir);
				FJsEnvRuntime::GetInstance().RestartJsScripts(JSScriptContentDir, TsScriptHomeRelativeDir, LaunchScriptPath, Arguments);
			}
		}
		else
		{
			UJsBridgeCaller::RemoveBridgeCaller(LaunchScriptPath);
			UE_LOG(LogReactorUMG, Error, TEXT("Can not obtain any valid javascript runtime environment"))
			return;
		}
		ReleaseJsEnv();
	}
	
	const bool DelegateRunResult = UJsBridgeCaller::ExecuteMainCaller(LaunchScriptPath, this->WidgetTree);
	if (!DelegateRunResult)
	{
		UJsBridgeCaller::RemoveBridgeCaller(LaunchScriptPath);
		ReleaseJsEnv();
		UE_LOG(LogReactorUMG, Warning, TEXT("Not bind any bridge caller for %s"), *LaunchScriptPath);
	}
}

void UReactorUIWidget::ReleaseJsEnv()
{
	if (JsEnv)
	{
		UE_LOG(LogReactorUMG, Display, TEXT("Release javascript env in order to excuting other script"))
		FJsEnvRuntime::GetInstance().ReleaseJsEnv(JsEnv);
		JsEnv = nullptr;
	}
}