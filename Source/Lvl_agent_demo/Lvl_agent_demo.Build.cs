// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Lvl_agent_demo : ModuleRules
{
	public Lvl_agent_demo(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"ChaosVehicles",
			"PhysicsCore",
			"UMG",
			"Slate",
            "SlateCore",
            "ImageWrapper",
            "MozaSDKPlugin",
            "RawInput",
            "Networking",
            "Sockets"
        });

		PublicIncludePaths.AddRange(new string[] {
			"Lvl_agent_demo",
			"Lvl_agent_demo/SportsCar",
			"Lvl_agent_demo/OffroadCar",
			"Lvl_agent_demo/Variant_Offroad",
			"Lvl_agent_demo/Variant_TimeTrial",
			"Lvl_agent_demo/Variant_TimeTrial/UI"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });
	}
}
