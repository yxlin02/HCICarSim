// Copyright WorldBLD LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetLibrary/WorldBLDAssetLibraryTypes.h"

struct FWorldBLDCategoryNode
{
	FString CategoryName;
	FString CategoryPath;
	FString ParentPath;
	TArray<FString> RequiredTags;
	TArray<TSharedPtr<FWorldBLDCategoryNode>> ChildCategories;

	bool MatchesCategory(const FWorldBLDAssetInfo& Asset) const
	{
		for (const FString& RequiredTag : RequiredTags)
		{
			if (!Asset.Tags.Contains(RequiredTag))
			{
				return false;
			}
		}
		return true;
	}

	bool MatchesCategory(const FWorldBLDKitInfo& Kit) const
	{
		for (const FString& RequiredTag : RequiredTags)
		{
			if (!Kit.Tags.Contains(RequiredTag))
			{
				return false;
			}
		}
		return true;
	}
};

class FWorldBLDCategoryHierarchy
{
public:
	static TArray<TSharedPtr<FWorldBLDCategoryNode>> BuildCategoryTree()
	{
		auto MakeNode = [](const FString& InName, const FString& InPath, const FString& InParentPath, const TArray<FString>& InRequiredTags) -> TSharedPtr<FWorldBLDCategoryNode>
		{
			TSharedPtr<FWorldBLDCategoryNode> Node = MakeShared<FWorldBLDCategoryNode>();
			Node->CategoryName = InName;
			Node->CategoryPath = InPath;
			Node->ParentPath = InParentPath;
			Node->RequiredTags = InRequiredTags;
			return Node;
		};

		TArray<TSharedPtr<FWorldBLDCategoryNode>> Roots;

		TSharedPtr<FWorldBLDCategoryNode> Home = MakeNode(TEXT("Home"), TEXT("Home"), TEXT(""), {});
		Roots.Add(Home);

		TSharedPtr<FWorldBLDCategoryNode> Collections = MakeNode(TEXT("Collections"), TEXT("Collections"), TEXT(""), {});
		Roots.Add(Collections);

		TSharedPtr<FWorldBLDCategoryNode> Assets = MakeNode(TEXT("Assets"), TEXT("Assets"), TEXT(""), {});

		TSharedPtr<FWorldBLDCategoryNode> Assemblies = MakeNode(TEXT("Assemblies"), TEXT("Assets/Assemblies"), TEXT("Assets"), { TEXT("Assemblies") });
		Assemblies->ChildCategories.Add(MakeNode(TEXT("Urban"), TEXT("Assets/Assemblies/Urban"), TEXT("Assets/Assemblies"), { TEXT("Assemblies"), TEXT("Urban") }));
		Assemblies->ChildCategories.Add(MakeNode(TEXT("Suburban"), TEXT("Assets/Assemblies/Suburban"), TEXT("Assets/Assemblies"), { TEXT("Assemblies"), TEXT("Suburban") }));
		Assemblies->ChildCategories.Add(MakeNode(TEXT("Rural"), TEXT("Assets/Assemblies/Rural"), TEXT("Assets/Assemblies"), { TEXT("Assemblies"), TEXT("Rural") }));
		Assets->ChildCategories.Add(Assemblies);

		TSharedPtr<FWorldBLDCategoryNode> CityBLDPresets = MakeNode(TEXT("CityBLD Presets"), TEXT("Assets/CityBLD Presets"), TEXT("Assets"), { TEXT("CityBLD Presets") });
		CityBLDPresets->ChildCategories.Add(MakeNode(TEXT("Building Styles"), TEXT("Assets/CityBLD Presets/Building Styles"), TEXT("Assets/CityBLD Presets"), { TEXT("CityBLD Presets"), TEXT("Building Styles") }));
		CityBLDPresets->ChildCategories.Add(MakeNode(TEXT("Districts"), TEXT("Assets/CityBLD Presets/Districts"), TEXT("Assets/CityBLD Presets"), { TEXT("CityBLD Presets"), TEXT("Districts") }));
		CityBLDPresets->ChildCategories.Add(MakeNode(TEXT("Prefabs"), TEXT("Assets/CityBLD Presets/Prefabs"), TEXT("Assets/CityBLD Presets"), { TEXT("CityBLD Presets"), TEXT("Prefabs") }));
		CityBLDPresets->ChildCategories.Add(MakeNode(TEXT("Land Uses"), TEXT("Assets/CityBLD Presets/Land Uses"), TEXT("Assets/CityBLD Presets"), { TEXT("CityBLD Presets"), TEXT("Land Uses") }));
		Assets->ChildCategories.Add(CityBLDPresets);

		TSharedPtr<FWorldBLDCategoryNode> RoadBLDPresets = MakeNode(TEXT("RoadBLD Presets"), TEXT("Assets/RoadBLD Presets"), TEXT("Assets"), { TEXT("RoadBLD Presets") });
		RoadBLDPresets->ChildCategories.Add(MakeNode(TEXT("Road Presets"), TEXT("Assets/RoadBLD Presets/Road Presets"), TEXT("Assets/RoadBLD Presets"), { TEXT("RoadBLD Presets"), TEXT("Road Presets") }));
		RoadBLDPresets->ChildCategories.Add(MakeNode(TEXT("Road Signs"), TEXT("Assets/RoadBLD Presets/Road Signs"), TEXT("Assets/RoadBLD Presets"), { TEXT("RoadBLD Presets"), TEXT("Road Signs") }));
		RoadBLDPresets->ChildCategories.Add(MakeNode(TEXT("Road Islands"), TEXT("Assets/RoadBLD Presets/Road Islands"), TEXT("Assets/RoadBLD Presets"), { TEXT("RoadBLD Presets"), TEXT("Road Islands") }));
		RoadBLDPresets->ChildCategories.Add(MakeNode(TEXT("Road Markings"), TEXT("Assets/RoadBLD Presets/Road Markings"), TEXT("Assets/RoadBLD Presets"), { TEXT("RoadBLD Presets"), TEXT("Road Markings") }));
		RoadBLDPresets->ChildCategories.Add(MakeNode(TEXT("Lane Types"), TEXT("Assets/RoadBLD Presets/Lane Types"), TEXT("Assets/RoadBLD Presets"), { TEXT("RoadBLD Presets"), TEXT("Lane Types") }));
		Assets->ChildCategories.Add(RoadBLDPresets);

		TSharedPtr<FWorldBLDCategoryNode> TwinBLDPresets = MakeNode(TEXT("TwinBLD Presets"), TEXT("Assets/TwinBLD Presets"), TEXT("Assets"), { TEXT("TwinBLD Presets") });
		Assets->ChildCategories.Add(TwinBLDPresets);

		TSharedPtr<FWorldBLDCategoryNode> CompleteWorlds = MakeNode(TEXT("Complete Worlds"), TEXT("Assets/Complete Worlds"), TEXT("Assets"), { TEXT("Complete Worlds") });
		Assets->ChildCategories.Add(CompleteWorlds);

		TSharedPtr<FWorldBLDCategoryNode> PCG = MakeNode(TEXT("PCG"), TEXT("Assets/PCG"), TEXT("Assets"), { TEXT("PCG") });
		Assets->ChildCategories.Add(PCG);

		TSharedPtr<FWorldBLDCategoryNode> Utilities = MakeNode(TEXT("Utilities"), TEXT("Assets/Utilities"), TEXT("Assets"), { TEXT("Utilities") });
		Assets->ChildCategories.Add(Utilities);

		TSharedPtr<FWorldBLDCategoryNode> Foliage = MakeNode(TEXT("Foliage"), TEXT("Assets/Foliage"), TEXT("Assets"), { TEXT("Foliage") });
		Foliage->ChildCategories.Add(MakeNode(TEXT("Trees"), TEXT("Assets/Foliage/Trees"), TEXT("Assets/Foliage"), { TEXT("Foliage"), TEXT("Trees") }));
		Foliage->ChildCategories.Add(MakeNode(TEXT("Bushes"), TEXT("Assets/Foliage/Bushes"), TEXT("Assets/Foliage"), { TEXT("Foliage"), TEXT("Bushes") }));
		Foliage->ChildCategories.Add(MakeNode(TEXT("Grass"), TEXT("Assets/Foliage/Grass"), TEXT("Assets/Foliage"), { TEXT("Foliage"), TEXT("Grass") }));
		Assets->ChildCategories.Add(Foliage);

		TSharedPtr<FWorldBLDCategoryNode> Props = MakeNode(TEXT("Props"), TEXT("Assets/Props"), TEXT("Assets"), { TEXT("Props") });
		Props->ChildCategories.Add(MakeNode(TEXT("Furniture"), TEXT("Assets/Props/Furniture"), TEXT("Assets/Props"), { TEXT("Props"), TEXT("Furniture") }));
		Props->ChildCategories.Add(MakeNode(TEXT("Urban"), TEXT("Assets/Props/Urban"), TEXT("Assets/Props"), { TEXT("Props"), TEXT("Urban") }));
		Props->ChildCategories.Add(MakeNode(TEXT("Vehicles"), TEXT("Assets/Props/Vehicles"), TEXT("Assets/Props"), { TEXT("Props"), TEXT("Vehicles") }));
		Props->ChildCategories.Add(MakeNode(TEXT("Signs"), TEXT("Assets/Props/Signs"), TEXT("Assets/Props"), { TEXT("Props"), TEXT("Signs") }));
		Assets->ChildCategories.Add(Props);

		Roots.Add(Assets);

		return Roots;
	}
};
