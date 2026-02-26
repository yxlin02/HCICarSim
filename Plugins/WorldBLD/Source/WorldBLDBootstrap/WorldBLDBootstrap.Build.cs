// Copyright WorldBLD LLC. All Rights Reserved.

using UnrealBuildTool;

public class WorldBLDBootstrap : ModuleRules
{
	public WorldBLDBootstrap(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// This module runs at PostConfigInit -- before most engine subsystems are ready.
		// Keep dependencies minimal: file I/O and JSON only.  No UObject, no Engine.
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Json",
				"Projects",
			}
		);
	}
}
