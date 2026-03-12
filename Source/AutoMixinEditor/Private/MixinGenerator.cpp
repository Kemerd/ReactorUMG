#include "MixinGenerator.h"
#include "ReactorUtils.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Misc/FileHelper.h"
#include "Widgets/Notifications/SNotificationList.h"

FString FMixinGenerator::MixinTSFilePath = FPaths::Combine(FReactorUtils::GetPluginDir(), TEXT("Resources"), TEXT("mixin.ts"));
FString FMixinGenerator::MixinTemplateFilePath = FPaths::Combine(FReactorUtils::GetPluginDir(), TEXT("Resources"), TEXT("MixinTemplate.ts"));
FString FMixinGenerator::StartGameTSFilePath = FPaths::Combine(FReactorUtils::GetGamePlayTSHomeDir(), TEXT("start_game.ts"));

void FMixinGenerator::InitMixinTSFile()
{
	if (!FPaths::FileExists(StartGameTSFilePath))
	{
		FFileHelper::SaveStringToFile(TEXT(""), *StartGameTSFilePath, FFileHelper::EEncodingOptions::ForceUTF8);
	}
	
	const FString TargetProjectMixinPath = FPaths::Combine(FReactorUtils::GetGamePlayTSHomeDir(), TEXT("mixin.ts"));
	if (!FPaths::FileExists(TargetProjectMixinPath) &&
		FPaths::FileExists(MixinTSFilePath))
	{
		FReactorUtils::CopyFile(MixinTSFilePath, TargetProjectMixinPath);
	}
}

FString FMixinGenerator::ProcessTemplate(const FString& TemplateContent, FString BlueprintPath, const FString& FileName)
{
	FString Result = TemplateContent;
	FString ProjectName = FApp::GetProjectName();
	
	// Get full Blueprint class name (with _C suffix)
	BlueprintPath += TEXT("_C");
	const FString BlueprintClass = TEXT("UE") + BlueprintPath.Replace(TEXT("/"), TEXT("."));

	const FString BLUEPRINT_PATH = TEXT("BLUEPRINT_PATH");
	const FString MIXIN_BLUEPRINT_TYPE = TEXT("MIXIN_BLUEPRINT_TYPE");
	const FString TS_NAME = TEXT("TS_NAME");
	const FString PROJECT_NAME = TEXT("PROJECT_NAME");

	Result = Result.Replace(*BLUEPRINT_PATH, *BlueprintPath);
	Result = Result.Replace(*MIXIN_BLUEPRINT_TYPE, *BlueprintClass);
	Result = Result.Replace(*TS_NAME, *FileName);
	Result = Result.Replace(*PROJECT_NAME, *ProjectName);

	return Result;
}

void FMixinGenerator::GenerateBPMixinFile(const UBlueprint* Blueprint)
{
	if (!Blueprint)
	{
		return;
	}
	
	const FString BlueprintPath = Blueprint->GetPathName();
	FString Lefts, Rights;
	BlueprintPath.Split(".", &Lefts, &Rights);

	// TypeScript file path
	FString TsFilePath = FReactorUtils::GetGamePlayTSHomeDir() + Lefts.Mid(5) /* exclude /Game */ + TEXT(".ts");

	// Skip if TS file already exists
	if (FPaths::FileExists(TsFilePath))
	{
		return;
	}

	// Parse Blueprint path to get filename
	TArray<FString> StringArray;
	Rights.ParseIntoArray(StringArray, TEXT("/"), false);
	const FString FileName = StringArray[StringArray.Num() - 1];

	// Read template file
	FString TemplateContent;
	if (FFileHelper::LoadFileToString(TemplateContent, *MixinTemplateFilePath))
	{
		// Process template and generate TS file content
		const FString TsContent = ProcessTemplate(TemplateContent, BlueprintPath, FileName);
		// Save generated content to file
		if (FFileHelper::SaveStringToFile(TsContent, *TsFilePath, FFileHelper::EEncodingOptions::ForceUTF8))
		{
			// Show notification
			FNotificationInfo Info(FText::Format(NSLOCTEXT("MiXinGenerator", "TsFileGenerated", "TS file generated successfully -> {0}"), FText::FromString(TsFilePath)));
			Info.ExpireDuration = 5.f;
			FSlateNotificationManager::Get().AddNotification(Info);
			
			FString MainGameTsContent;
			if (FFileHelper::LoadFileToString(MainGameTsContent, *StartGameTSFilePath))
			{
				const FString TsPath = Lefts.Mid(5);
				if (!MainGameTsContent.Contains(TEXT("import \".") + TsPath + "\";"))
				{
					MainGameTsContent += TEXT("import \"."+TsPath + "\";\n");
					FFileHelper::SaveStringToFile(MainGameTsContent, *StartGameTSFilePath, FFileHelper::EEncodingOptions::ForceUTF8);
					UE_LOG(LogTemp, Log, TEXT("MainGame.ts updated successfully"));
				}
			} else
			{
				MainGameTsContent += TEXT("import \"."+ Lefts.Mid(5) + "\";\n");
				FFileHelper::SaveStringToFile(MainGameTsContent, *StartGameTSFilePath, FFileHelper::EEncodingOptions::ForceUTF8);
				UE_LOG(LogTemp, Log, TEXT("MainGame.ts updated successfully"));
			}
		}
	}
	else
	{
		// Log warning if template file doesn't exist
		UE_LOG(LogTemp, Warning, TEXT("MixinTemplate.ts does not exist"));
	}
}
