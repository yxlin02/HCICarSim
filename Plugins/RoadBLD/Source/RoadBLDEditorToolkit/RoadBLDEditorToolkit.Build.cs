// Copyright WorldBLD LLC. All rights reserved.

using UnrealBuildTool;

public class RoadBLDEditorToolkit : ModuleRules
{
	public RoadBLDEditorToolkit(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		bUsePrecompiled = true;
        

        PublicDependencyModuleNames.AddRange(new string[]
		{
			// Core Engine Dependencies (keep Alphabetical please)
			"ApplicationCore",
			"AssetManagerEditor",
			"AssetRegistry",
			"AssetTools",
			"BlueprintGraph",
			"Blutility",
			"ComponentVisualizers",
			"ContentBrowser",
			"ContentBrowserData",
			"Core",
			"CoreUObject",
			"DetailCustomizations",
			"DeveloperSettings",
			"DynamicMesh",
			"EditorFramework",
			"EditorScriptingUtilities",
			"EditorStyle",
			"EditorSubsystem",
			"EditorWidgets",
			"Engine",
			"GameplayTags",
			"GeometryFramework",
			"GeometryScriptingEditor",
			"HTTP",
			"ImageWriteQueue",
			"ImageWrapper",
			"InputCore",
			"InteractiveToolsFramework",
			"Json",
            "JsonUtilities",
			"LevelEditor",
			"ProceduralMeshComponent",
			"Projects",
			"RenderCore",
			"RHI",
			"RoadBLDRuntime",
			"Slate",
			"SlateCore",
			"Settings",
			"UnrealEd",
			"UMG",
			"UMGEditor",
			"WorldBLDEditor",
            "WorldBLDRuntime"
        });

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			// Internal Module Dependencies (from this plugin)
			"GeometryScriptingCore",
			// Engine Dependencies
			"OpenSSL",
			"PlatformCryptoContext",
		});

        PrivateIncludePaths.AddRange(new string[] {
            System.IO.Path.Combine(GetModuleDirectory("Slate"), "Private"),
            // RoadController.cpp uses some WorldBLD editor widgets declared in WorldBLDEditor's Private headers.
            // This module is editor-only, so this is acceptable here.
            System.IO.Path.Combine(GetModuleDirectory("WorldBLDEditor"), "Private"),
        });

        AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenSSL");

// [PROD-REM] START
        PrivateDefinitions.Add("CITYBLD_LANE_CHECKS=1");
// [PROD-REM] END
    }
}
