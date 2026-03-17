#include "ReactorBlueprintCompilerContext.h"

#include "ReactorUMGWidgetBlueprint.h"
#include "ReactorUMGBlueprintGeneratedClass.h"
#include "ReactorUMGUtilityWidgetBlueprint.h"
#include "Kismet2/KismetReinstanceUtilities.h"
#include "Async/Async.h"

FReactorUMGBlueprintCompilerContext::FReactorUMGBlueprintCompilerContext(UWidgetBlueprint* SourceBlueprint,
	FCompilerResultsLog& InMessageLog, const FKismetCompilerOptions& InCompilerOptions)
	: Super(SourceBlueprint, InMessageLog, InCompilerOptions)
{
	
}

FReactorUMGBlueprintCompilerContext::~FReactorUMGBlueprintCompilerContext()
{
	
}

void FReactorUMGBlueprintCompilerContext::EnsureProperGeneratedClass(UClass*& TargetUClass)
{
	if (TargetUClass && !((UObject*)TargetUClass)->IsA(UReactorUMGBlueprintGeneratedClass::StaticClass()))
	{
		FKismetCompilerUtilities::ConsignToOblivion(TargetUClass, Blueprint->bIsRegeneratingOnLoad);
		TargetUClass = nullptr;
	}
}

void FReactorUMGBlueprintCompilerContext::SpawnNewClass(const FString& NewClassName)
{
	UReactorUMGBlueprintGeneratedClass* BlueprintGeneratedClass = FindObject<UReactorUMGBlueprintGeneratedClass>(Blueprint->GetOutermost(), *NewClassName);

	if (BlueprintGeneratedClass == nullptr)
	{
		BlueprintGeneratedClass = NewObject<UReactorUMGBlueprintGeneratedClass>(Blueprint->GetOutermost(), FName(*NewClassName), RF_Public | RF_Transactional);
	}
	else
	{
		FBlueprintCompileReinstancer::Create(BlueprintGeneratedClass);
	}

	NewClass = BlueprintGeneratedClass;
}

void FReactorUMGBlueprintCompilerContext::CopyTermDefaultsToDefaultObject(UObject* DefaultObject)
{
	
}

void FReactorUMGBlueprintCompilerContext::FinishCompilingClass(UClass* Class)
{
	if (Class)
	{
		UE_LOG(LogTemp, Log, TEXT("FinishCompilingClass %s"), *Class->GetName())
	}

	// Skeleton classes (SKEL_*) are intermediate Kismet artifacts used for
	// dependency resolution during Blueprint compilation.  They don't need
	// TypeScript compilation or JS module reloads — skip them entirely.
	const bool bIsSkeletonClass = Class && Class->GetName().StartsWith(TEXT("SKEL_"));

	if (UReactorUMGWidgetBlueprint* WidgetBlueprint = Cast<UReactorUMGWidgetBlueprint>(Blueprint))
	{
		// Path metadata is cheap — always safe to set immediately
		UReactorUMGBlueprintGeneratedClass* BPGClass = CastChecked<UReactorUMGBlueprintGeneratedClass>(Class);
		if (BPGClass)
		{
			BPGClass->MainScriptPath = WidgetBlueprint->GetMainScriptPath();
			BPGClass->TsProjectDir = WidgetBlueprint->GetTsProjectDir();
			BPGClass->TsScriptHomeFullDir = WidgetBlueprint->GetTsScriptHomeFullDir();
			BPGClass->TsScriptHomeRelativeDir = WidgetBlueprint->GetTsScriptHomeRelativeDir();
			BPGClass->WidgetName = WidgetBlueprint->GetWidgetName();
		}

		if (!bIsSkeletonClass)
		{
			// Defer the expensive TS compile + JS reload to the next game-thread
			// task pump.  This avoids blocking the Kismet compiler for ~2 seconds
			// (which triggers the stall detector) and prevents access violations
			// from V8 touching UObjects that are still mid-compilation.
			TWeakObjectPtr<UReactorUMGWidgetBlueprint> WeakBP = WidgetBlueprint;
			AsyncTask(ENamedThreads::GameThread, [WeakBP]()
			{
				if (UReactorUMGWidgetBlueprint* BP = WeakBP.Get())
				{
					BP->SetupTsScriptsDeferred(/*bForceCompile=*/ true, /*bForceReload=*/ true);
				}
			});
		}
	}

	if (UReactorUMGUtilityWidgetBlueprint* UtilityWidgetBlueprint = Cast<UReactorUMGUtilityWidgetBlueprint>(Blueprint))
	{
		UReactorUMGBlueprintGeneratedClass* BPGClass = CastChecked<UReactorUMGBlueprintGeneratedClass>(Class);
		if (BPGClass)
		{
			BPGClass->MainScriptPath = UtilityWidgetBlueprint->GetMainScriptPath();
			BPGClass->TsProjectDir = UtilityWidgetBlueprint->GetTsProjectDir();
			BPGClass->TsScriptHomeFullDir = UtilityWidgetBlueprint->GetTsScriptHomeFullDir();
			BPGClass->TsScriptHomeRelativeDir = UtilityWidgetBlueprint->GetTsScriptHomeRelativeDir();
			BPGClass->WidgetName = UtilityWidgetBlueprint->GetWidgetName();
		}

		if (!bIsSkeletonClass)
		{
			TWeakObjectPtr<UReactorUMGUtilityWidgetBlueprint> WeakBP = UtilityWidgetBlueprint;
			AsyncTask(ENamedThreads::GameThread, [WeakBP]()
			{
				if (UReactorUMGUtilityWidgetBlueprint* BP = WeakBP.Get())
				{
					BP->SetupTsScriptsDeferred(/*bForceCompile=*/ true, /*bForceReload=*/ true);
				}
			});
		}
	}

	Super::FinishCompilingClass(Class);
}