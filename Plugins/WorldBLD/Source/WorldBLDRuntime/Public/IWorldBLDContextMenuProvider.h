// Copyright WorldBLD LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "IWorldBLDContextMenuProvider.generated.h"

class AActor;

UINTERFACE(MinimalAPI, BlueprintType)
class UWorldBLDContextMenuProvider : public UInterface
{
	GENERATED_BODY()
};

class WORLDBLDRUNTIME_API IWorldBLDContextMenuProvider
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, CallInEditor, Category = "WorldBLD|ContextMenu")
	AActor* GetContextMenuTargetActor() const;

protected:
	virtual AActor* GetContextMenuTargetActor_Implementation() const { return nullptr; }
};
