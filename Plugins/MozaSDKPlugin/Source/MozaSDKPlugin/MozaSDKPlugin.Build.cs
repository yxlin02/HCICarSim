// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class MozaSDKPlugin : ModuleRules
{
	public MozaSDKPlugin(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		bUseRTTI = true;
		bEnableExceptions = true;

		

		string PluginDir = Path.GetFullPath(Path.Combine(ModuleDirectory, "../../"));
		string SDKPath = Path.Combine(PluginDir, "ThirdParty", "MozaSDK");

		PublicIncludePaths.Add(Path.Combine(SDKPath, "include"));
		PublicAdditionalLibraries.Add(Path.Combine(SDKPath, "lib", "MOZA_SDK.lib"));

		RuntimeDependencies.Add("$(PluginDir)/ThirdParty/MozaSDK/bin/MOZA_API_C.dll");
		PublicDelayLoadDLLs.Add("MOZA_API_C.dll");

	
		
		
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"InputCore",
               

				
				// ... add other public dependencies that you statically link with here ...
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				

				// ... add private dependencies that you statically link with here ...	
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
