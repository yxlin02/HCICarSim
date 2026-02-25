// Copyright 2025 WorldBLD. All rights reserved.

using UnrealBuildTool;

public class CityBLDRuntime : ModuleRules
{
	public CityBLDRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		bUsePrecompiled = true;
		PrecompileForTargets = PrecompileTargetsType.Any;
		//OptimizeCode = CodeOptimization.Never;
		bUseUnity = false;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			// Core Engine Dependencies (keep Alphabetical please)
			"Core",
			"CoreUObject",
            "DeveloperSettings",
            "DynamicMesh",
			"Engine",
			"GameplayTags",
			"InputCore",
            "Landscape",
			"MeshConversion",
			"PCG",
			"PhysicsCore",
            "ProceduralMeshComponent",
            "UMG",
            "Slate",
			"SlateCore",			
			// Internal Dependencies,
			"WorldBLDRuntime",
            "GeometryScriptingCore"
            
        });

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			// Core Engine Dependencies (keep Alphabetical please)
			"GeometryAlgorithms",
			"GeometryCore",
			"GeometryFramework",
			"GeometryScriptingCore",
			"MeshDescription",
			"ModelingComponents",
			"Projects",
			"RenderCore",
			"StaticMeshDescription",
			
        });

		// Allow for editor-specific functionality
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[] 
			{
				"ComponentVisualizers",
				"EditorScriptingUtilities",
				"EditorSubsystem",
                "Blutility",
				"ModelingComponentsEditorOnly",
                "UMGEditor",
                "SubobjectDataInterface",
				"TypedElementFramework",
				"TypedElementRuntime",				
				"UnrealEd",
			});
		}
	}
}
