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
	/* Make this widget focusable so it can receive keyboard events
	 * and participate in Slate's focus navigation system. The React
	 * event dispatcher on the TS side handles routing key events
	 * to the currently focused React component. */
	bIsFocusable = true;
}

bool UReactorUIWidget::Initialize()
{
	bool SuperRes = Super::Initialize();
	SetNewWidgetTree();
	return SuperRes;
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
		UReactorUMGBlueprintGeneratedClass* ReactorBPGC = Cast<UReactorUMGBlueprintGeneratedClass>(GetClass());
		if (ReactorBPGC)
		{
			TsProjectDir = ReactorBPGC->TsProjectDir;
			TsScriptHomeRelativeDir = ReactorBPGC->TsScriptHomeRelativeDir;
			LaunchScriptPath = ReactorBPGC->MainScriptPath;
			if (!LaunchScriptPath.IsEmpty())
			{
				/* If the UMG designer owns the widget tree (Designing flag),
				 * skip script execution to avoid clobbering designer state.
				 * Mirrors the safety check in ReactorUtilityWidget. */
				if (this->WidgetTree && WidgetTree->RootWidget)
				{
					auto RootDesignerFlags = this->WidgetTree->RootWidget->GetDesignerFlags();
					if (!EnumHasAnyFlags(RootDesignerFlags, EWidgetDesignFlags::Designing))
					{
						this->WidgetTree->RootWidget->RemoveFromParent();
						this->WidgetTree->RootWidget->MarkAsGarbage();
						this->WidgetTree->RootWidget = nullptr;
						RunScriptToInitWidgetTree();
					}
				}
				else
				{
					/* No existing root widget -- fresh tree, safe to run script */
					if (WidgetTree != nullptr)
					{
						UWidgetTree* OldWidgetTree = WidgetTree;
						WidgetTree = NewObject<UWidgetTree>(this, NAME_None, RF_Transient);
						OldWidgetTree->MarkAsGarbage();
					}
					RunScriptToInitWidgetTree();
				}
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

/* ====================================================================== */
/*  Input Event Overrides                                                  */
/*                                                                         */
/*  These NativeOn* overrides intercept Slate input events at the          */
/*  UserWidget level. They return Unhandled by default so that child       */
/*  widgets (and Slate's normal routing) still work correctly. The React   */
/*  event dispatcher on the TS side handles the actual event propagation   */
/*  through the React component tree via Border/Button delegates.          */
/*                                                                         */
/*  For keyboard events specifically, these catch-all overrides ensure     */
/*  that key events which bubble past all child widgets still reach the    */
/*  React event system for components listening via onKeyDown/onKeyUp.     */
/* ====================================================================== */

FReply UReactorUIWidget::NativeOnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	/* Let the blueprint/parent implementation run first (handles
	 * any user-defined Blueprint key bindings). */
	FReply SuperReply = Super::NativeOnKeyDown(MyGeometry, InKeyEvent);
	if (SuperReply.IsEventHandled())
	{
		return SuperReply;
	}

	/* Tab navigation: if the React event system handles Tab,
	 * consume the event to prevent Slate's default focus shift. */
	if (InKeyEvent.GetKey() == EKeys::Tab)
	{
		/* Tab handling is done on the TS side via routeTabKey().
		 * We return Unhandled here and let the TS side decide. */
	}

	/* Return Unhandled so child widgets can still process keys.
	 * The React event dispatcher receives key events via the
	 * JS-side routeKeyEvent() function which is called from
	 * the TS bridge when needed. */
	return FReply::Unhandled();
}

FReply UReactorUIWidget::NativeOnKeyUp(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	FReply SuperReply = Super::NativeOnKeyUp(MyGeometry, InKeyEvent);
	if (SuperReply.IsEventHandled())
	{
		return SuperReply;
	}

	return FReply::Unhandled();
}

FReply UReactorUIWidget::NativeOnFocusReceived(const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent)
{
	/* Accept focus so keyboard events can route to this widget */
	return FReply::Handled();
}

void UReactorUIWidget::NativeOnFocusLost(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnFocusLost(InFocusEvent);
}

FReply UReactorUIWidget::NativeOnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	/* Let the default routing handle mouse events via child widgets.
	 * The React event dispatcher binds to Border/Button delegates
	 * for mouse event interception at the component level. */
	return FReply::Unhandled();
}

FReply UReactorUIWidget::NativeOnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return FReply::Unhandled();
}

FReply UReactorUIWidget::NativeOnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return FReply::Unhandled();
}

FReply UReactorUIWidget::NativeOnTouchStarted(const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent)
{
	return FReply::Unhandled();
}

FReply UReactorUIWidget::NativeOnTouchMoved(const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent)
{
	return FReply::Unhandled();
}

FReply UReactorUIWidget::NativeOnTouchEnded(const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent)
{
	return FReply::Unhandled();
}

void UReactorUIWidget::NativeOnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& PointerEvent, UDragDropOperation*& Operation)
{
	/* Drag detection is handled at the per-widget level through
	 * the React event system. This override ensures the widget
	 * participates in Slate's drag detection pipeline. */
	Super::NativeOnDragDetected(MyGeometry, PointerEvent, Operation);
}

void UReactorUIWidget::NativeOnDragEnter(const FGeometry& MyGeometry, const FDragDropEvent& PointerEvent, UDragDropOperation* Operation)
{
	Super::NativeOnDragEnter(MyGeometry, PointerEvent, Operation);
}

void UReactorUIWidget::NativeOnDragLeave(const FDragDropEvent& PointerEvent, UDragDropOperation* Operation)
{
	Super::NativeOnDragLeave(PointerEvent, Operation);
}

bool UReactorUIWidget::NativeOnDrop(const FGeometry& MyGeometry, const FDragDropEvent& PointerEvent, UDragDropOperation* Operation)
{
	return Super::NativeOnDrop(MyGeometry, PointerEvent, Operation);
}
