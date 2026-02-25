using System;
using System.IO;
using UnrealBuildTool;

namespace UnrealBuildTool.Rules
{
	public class ClothoidsModule : ModuleRules
	{
        public ClothoidsModule(ReadOnlyTargetRules Target) : base(Target)
		{
			PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
			bUsePrecompiled = true;
            PrecompileForTargets = PrecompileTargetsType.Any;
            //bPrecompile = true;
			
			string BaseDir = Path.GetDirectoryName(RulesCompiler.GetFileNameFromType(GetType()));
			string ModuleDir = Path.Combine(BaseDir, "ClothoidsModule");
			PublicIncludePaths.Add(Path.Combine(BaseDir, "PolynomialRoots/src"));
			PublicIncludePaths.Add(Path.Combine(BaseDir, "PolynomialRoots/include"));			

            PrivateDependencyModuleNames.AddRange(
				new string[] {
                    "Core",
					"CoreUObject",
					"Engine",
					"GeometryAlgorithms",
					"GeometryCore",
					"GeometryFramework",
					"GeometryScriptingCore",
					"RHI",
					"RenderCore",
                    "NavigationSystem"
                }
			);
		}
	}
}