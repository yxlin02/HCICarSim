// Copyright WorldBLD LLC. All rights reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Interfaces/IPluginManager.h"
#include "WorldBLDKitPlugin.generated.h"

 struct FAssetData;

USTRUCT(BlueprintType)
struct FWorldBLDKitPluginReference
{
	GENERATED_BODY()

	// The name of the plugin.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="RoadBLD")
	FString PluginName;

	// The literal content root path.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="RoadBLD")
	FString MountedContentRoot;

	// The associated bundle with this plugin.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="RoadBLD")
	class UWorldBLDKitBundle* WorldBLDKitBundle {nullptr};

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="RoadBLD")
	bool bIsContentBased {false};

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="RoadBLD")
	FString AssetPath;

	TSharedPtr<IPlugin> GetPlugin();
	TSharedPtr<IPlugin> GetPlugin() const;

	void ProcessPlugin(TSharedPtr<IPlugin> Plugin);
	void ProcessContentBundle(const FString& InAssetPath);
	static FString GetPluginWorldBLDKitPath(TSharedPtr<IPlugin> Plugin);
	static bool PluginIsWorldBLDKit(TSharedPtr<IPlugin> Plugin);

private:
	TWeakPtr<IPlugin> PluginHandle;
};

UCLASS()
class UWorldBLDKitPluginUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	///////////////////////////////////////////////////////////////////////////////////////////////

	// Returns all the WorldBLDKit plugins currently loaded by the project.
	UFUNCTION(BlueprintCallable, Category="WorldBLDEditor|WorldBLDKits")
	static TArray<FWorldBLDKitPluginReference> GetAllWorldBLDKitPlugins();

	UFUNCTION(BlueprintPure, Category="WorldBLDEditor", Meta=(DisplayName="Get WorldBLD Version String"))
	static FString GetWorldBLDVersionString();

	UFUNCTION(BlueprintCallable, Category="WorldBLDEditor|WorldBLDKits")
	static void EnsureWorldBLDKitPluginLoaded(UPARAM(ref) FWorldBLDKitPluginReference& PluginRef);

	// Returns the version string of the plugin by reference.
	UFUNCTION(BlueprintPure, Category="WorldBLDEditor|WorldBLDKits")
	static FString GetWorldBLDKitPluginVersionString(const FWorldBLDKitPluginReference& PluginRef);
	
	UFUNCTION(BlueprintCallable, Category="WorldBLDEditor|WorldBLDKits")
	static bool FindWorldBLDKitPluginForBundle(UWorldBLDKitBundle* Bundle, FWorldBLDKitPluginReference& OutPluginRef);

};
