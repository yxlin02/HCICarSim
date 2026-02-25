// Copyright 2025 WorldBLD. All rights reserved.

using System.IO;
using UnrealBuildTool;

public class RoadBLDRuntime : ModuleRules
{
	public RoadBLDRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		bUsePrecompiled = true;
		PrecompileForTargets = PrecompileTargetsType.Any;
		//OptimizeCode = CodeOptimization.Never;
		bUseUnity = false;

		string ThirdPartyIncludePath = Path.Combine(ModuleDirectory, "ThirdParty", "include");
		if (Directory.Exists(ThirdPartyIncludePath))
		{
			PublicIncludePaths.Add(ThirdPartyIncludePath);
		}

        PublicDependencyModuleNames.AddRange(new string[]
		{
			// Core Engine Dependencies (keep Alphabetical please)
			"Core",
			"CoreUObject",
            "DeveloperSettings",
            "DynamicMesh",
			"Engine",
			"GeometryScriptingCore",
			"GameplayTags",
			"InputCore",
            "Landscape",
			"MeshDescription",
			"PhysicsCore",
            "ProceduralMeshComponent",
			"Slate",
			"SlateCore",
			"StaticMeshDescription",
			// Internal Dependencies
			"ClothoidsModule",
			"WorldBLDRuntime", 
			"XmlParser"
		});

	PrivateDependencyModuleNames.AddRange(new string[]
	{
		// Core Engine Dependencies (keep Alphabetical please)
		"GeometryAlgorithms",
		"GeometryCore",
		"GeometryFramework",
		"MeshConversion",
		"ModelingComponents",
		"Projects",
		"RenderCore",
		
    });

	// Allow for editor-specific functionality
	if (Target.Type == TargetType.Editor) 
	{
		PrivateDependencyModuleNames.AddRange(new string[] 
		{
			"ComponentVisualizers",
			"EditorScriptingUtilities",
			"EditorSubsystem",
			"MaterialBaking",
			"MeshBuilder",
			"MeshMergeUtilities",
			"ModelingComponentsEditorOnly",
			"SubobjectDataInterface",
			"TypedElementFramework",
			"TypedElementRuntime",
			"UnrealEd",
		});
	}
	}
}
