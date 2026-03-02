// Copyright WorldBLD LLC. All Rights Reserved.

using UnrealBuildTool;

public class WorldBLDEditor : ModuleRules
{
	public WorldBLDEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
				System.IO.Path.Combine(ModuleDirectory, "Public"),
				System.IO.Path.Combine(ModuleDirectory, "Public", "AssetLibrary"),
				System.IO.Path.Combine(ModuleDirectory, "Public", "LevelTemplate"),
				System.IO.Path.Combine(ModuleDirectory, "Public", "ContextMenu"),
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
				System.IO.Path.Combine(ModuleDirectory, "Private"),
				System.IO.Path.Combine(ModuleDirectory, "Private", "AssetLibrary"),
				System.IO.Path.Combine(ModuleDirectory, "Private", "LevelTemplate"),
				System.IO.Path.Combine(ModuleDirectory, "Private", "ContextMenu"),
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"ApplicationCore",
				"AssetManagerEditor",
				"AssetRegistry",
				"AssetTools",
				"BlueprintGraph",
				"Blutility",
				"Core",
				"ComponentVisualizers",
				"ContentBrowser",
				"ContentBrowserData",
				"CoreUObject",
				"DetailCustomizations",
				"DeveloperSettings",
				"EditorFramework",
				"EditorScriptingUtilities",
				"EditorStyle",
				"EditorSubsystem",
				"EditorWidgets",
				"Engine",
				"GameplayTags",
				"HTTP",
				"ImageCore",
				"ImageWrapper",
				"InputCore",
				"InteractiveToolsFramework",
				"Json",
				"JsonUtilities",
				"Landscape",
				"LandscapeEditor",
				"LevelEditor",
				"Projects",
				"ScriptableEditorWidgets",
				"Slate",
				"SlateCore",
				"Settings",
				"ToolWidgets",
				"UnrealEd",
				"UMG",
				"UMGEditor"
				// ... add other public dependencies that you statically link with here ...
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"EditorInteractiveToolsFramework",
				"ToolMenus",
				"EditorSubsystem",
				"LevelEditor",
				"WorldBLDRuntime",

				// Engine Dependencies
				"OpenSSL",
				"PlatformCryptoContext",
				// ... add private dependencies that you statically link with here ...
			}
			);
		
		PrivateIncludePaths.AddRange(new string[] {
			System.IO.Path.Combine(GetModuleDirectory("Slate"), "Private"),
		});
		PrivateDefinitions.Add("WITH_LIBCURL=1"); 
		PrivateDefinitions.Add("CURL_STATICLIB=1");
		
		AddEngineThirdPartyPrivateStaticDependencies(Target, new string[]
		{
			"libcurl",
			"nghttp2",
			"OpenSSL",
			"zlib"  // zlib includes minizip headers automatically
		});
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);

// [PROD-REM] START
		PrivateDefinitions.Add("CITYBLD_LANE_CHECKS=1");
// [PROD-REM] END
	}
}
