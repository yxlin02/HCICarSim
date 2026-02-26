// Copyright WorldBLD LLC. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class WorldBLDRuntime : ModuleRules
{
	public WorldBLDRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
								
		PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "ThirdParty/include"));		
		PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "ThirdParty/src"));

		// UE 5.7+ defaults UndefinedIdentifierWarningLevel to Error (BuildSettings V6).
		// Clipper2 uses `#if CLIPPER2_HI_PRECISION`, so define it explicitly for consistent builds.
		PublicDefinitions.Add("CLIPPER2_HI_PRECISION=0");
		//PublicDefinitions.Add("CLIPPER2_EXPORTS=1");
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				// Core Engine Dependencies (keep Alphabetical please)
				"Core",
				"CoreUObject",
				"DeveloperSettings",
				"Engine",
				"GameplayTags",
				"GeometryAlgorithms",
				"GeometryCore",
				"GeometryFramework",
				"InputCore",
				"MeshDescription",
				"PhysicsCore",
				"Slate",
				"SlateCore",
				"StaticMeshDescription",
			}
			);
			
		
	PrivateDependencyModuleNames.AddRange(
		new string[]
		{
			// Core Engine Dependencies (keep Alphabetical please)
			"Projects",
			"RenderCore",
		}
		);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
		
		// Allow for editor-specific functionality (must not be referenced for non-editor targets)
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[] 
			{
				"EditorScriptingUtilities",
				"EditorStyle",
				"EditorSubsystem",
				"EditorWidgets",
				"GeometryScriptingCore",
				"MeshBuilder",
				"MeshConversion",
				"ModelingComponents",
				"SubobjectDataInterface",
				"TypedElementFramework",
				"TypedElementRuntime",
				"UnrealEd",
			});
		}
	}
}
