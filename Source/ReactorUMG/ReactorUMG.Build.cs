using UnrealBuildTool;
using System.IO;

public class ReactorUMG : ModuleRules
{
	public ReactorUMG(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"UMG",
				"Engine",
				"Slate",
				"SlateCore",
				"InputCore",
				"Projects",
				"DeveloperSettings",
				"JsEnv",
				"HTTP",
				"Json",
				"JsonUtilities",
				"ProceduralMeshComponent",
				"Puerts",
			}
		);

		// -----------------------------------------------------------
		// SpinePlugin: only link if the module source directory exists
		// -----------------------------------------------------------
		string SpinePluginDir = Path.Combine(ModuleDirectory, "..", "Spine", "SpinePlugin");
		bool bHasSpinePlugin = Directory.Exists(SpinePluginDir);
		if (bHasSpinePlugin)
		{
			PrivateDependencyModuleNames.Add("SpinePlugin");
			PublicDefinitions.Add("WITH_SPINE_PLUGIN=1");
		}
		else
		{
			PublicDefinitions.Add("WITH_SPINE_PLUGIN=0");
		}
	}
}
