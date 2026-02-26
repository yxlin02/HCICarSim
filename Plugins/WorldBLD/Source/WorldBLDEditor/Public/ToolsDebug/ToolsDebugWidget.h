// Copyright WorldBLD LLC. All rights reserved. 

#pragma once

#include "EditorUtilityWidget.h"
#include "WorldBLDEditController.h"
#include "LevelEditor.h"
#include "LevelEditorViewport.h"
#include "SLevelViewport.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Components/DetailsView.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"
#include "SourceCodeNavigation.h"
#include "ToolsDebugWidget.generated.h"


UINTERFACE(MinimalAPI)
class UToolsDebugInterface : public UInterface
{
	GENERATED_BODY()
};

class WORLDBLDEDITOR_API IToolsDebugInterface
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Tool")
	void GetCategoriesToShow(TArray<FName>& CategoriesToShow);

};

UCLASS(Blueprintable, BlueprintType)
class WORLDBLDEDITOR_API UToolsDebugWidget : public UEditorUtilityWidget
{
	GENERATED_BODY()

public:

	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category = "Tool")
	UDetailsView* DetailsViewSelf;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category = "Tool")
	UDetailsView* DetailsViewTool;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tool")
	TSubclassOf<UWorldBLDEditController> Subtool;

	UPROPERTY(BlueprintReadWrite, Category = "Tool")
	UWorldBLDEditController* ActiveSubtool {nullptr};

	UPROPERTY(BlueprintReadOnly, Category = "Tool")
	TArray<UWorldBLDEditController*> Subtools;

	UPROPERTY(BlueprintReadWrite, Category = "Tool")
	bool bOverrideCategoriesToShow {false};

	UPROPERTY(BlueprintReadWrite, Category = "Tool")
	bool bShowOnlyChildCategories {true};

	UPROPERTY(BlueprintReadWrite, Category = "Tool")
	TArray<FName> CategoriesToShowOverride;

	UFUNCTION(BlueprintCallable, Category = "Tool")
	void ClearActiveSubtool()
	{
		if (IsValid(ActiveSubtool))
		{
			GEditor->OnBlueprintPreCompile().RemoveAll(this);
			RemoveFromEditorLevelViewport();
			if (Subtools.Contains(ActiveSubtool))
			{
				Subtools.Remove(ActiveSubtool);
			}
			ActiveSubtool->Destruct();
			ActiveSubtool = nullptr;
		}
	}

	UFUNCTION(BlueprintCallable, Category = "Tool")
	void SetActiveSubtool(TSubclassOf<UWorldBLDEditController> SubtoolClass = nullptr)
	{
		if (!IsValid(SubtoolClass))
		{
			ClearActiveSubtool();
			return;
		}
		if (IsValid(ActiveSubtool))
		{
			if (ActiveSubtool->GetClass() == SubtoolClass)
			{
				return;
			}
			else
			{
				ClearActiveSubtool();
			}
		}

		if (UWorldBLDEditController** ExistingTool = Subtools.FindByPredicate([&](UWorldBLDEditController* Subtool) {			
				return IsValid(Subtool) && Subtool->GetClass() == SubtoolClass.Get(); }))
		{
			ActiveSubtool = *ExistingTool;
		}
		else
		{
			//TryConstructUserWidget(ActiveSubtool, Subtool, true);
			ActiveSubtool = CreateWidget<UWorldBLDEditController>(this, SubtoolClass);
			if (UBlueprint* Blueprint = UBlueprint::GetBlueprintFromClass(SubtoolClass.Get()))
			{
				GEditor->OnBlueprintPreCompile().AddUObject(this, &UToolsDebugWidget::OnSubtoolCompiled);
				Subtools.Push(ActiveSubtool);
			}
		}

		AddToEditorLevelViewport();
	}

	UFUNCTION(BlueprintNativeEvent)
	void OnSubtoolChanged();
	
	UFUNCTION(BlueprintCallable, Category = "Tool")
	void NavigateToClassSource(const UClass* Class)
	{
		if (Class)
		{
			if (const UClass* ClassObj = Cast<UClass>(Class)) FSourceCodeNavigation::NavigateToClass(ClassObj);
			if (const UClass* ClassObj = Class->GetClass()) FSourceCodeNavigation::NavigateToClass(ClassObj);
		}
	}

protected:

	virtual void NativeDestruct() override
	{
		ClearActiveSubtool();
		for (int i = Subtools.Num() - 1; i >= 0; i--)
		{
			UWorldBLDEditController* Tool = Subtools[i];
			if (IsValid(Tool))
			{
				Tool->RemoveFromParent();
			}
		}
		Subtools.Empty();
		Super::NativeDestruct();
	}

	virtual TSharedRef<SWidget> RebuildWidget() override
	{
		TSharedRef<SWidget> Widget = Super::RebuildWidget();
		if (IsValid(DetailsViewSelf))
		{
			DetailsViewSelf->CategoriesToShow = { TEXT("Tool") };
			DetailsViewSelf->SetObject(this);
		}
		return Widget;
	}

	virtual void OnSubtoolChanged_Implementation()
	{
		if (IsValid(ActiveSubtool) && IsValid(DetailsViewTool))
		{
			if (UKismetSystemLibrary::DoesImplementInterface(ActiveSubtool, UToolsDebugInterface::StaticClass()))
			{
				IToolsDebugInterface::Execute_GetCategoriesToShow(ActiveSubtool, DetailsViewTool->CategoriesToShow);				
			}
			else
			{
				DetailsViewTool->CategoriesToShow.Reset();
				GetBlueprintOwnCategoryNames(ActiveSubtool->GetClass(), DetailsViewTool->CategoriesToShow, bShowOnlyChildCategories);
			}
			if (bOverrideCategoriesToShow)
			{
				DetailsViewTool->CategoriesToShow = CategoriesToShowOverride;
			}
			if (DetailsViewTool->CategoriesToShow.IsEmpty())
			{
				DetailsViewTool->CategoriesToShow = { TEXT("None") };
			}
			DetailsViewTool->SetObject(ActiveSubtool);
		}
		else if (IsValid(DetailsViewTool))
		{
			DetailsViewTool->SetObject(nullptr);
		}
	}

	void OnSubtoolCompiled(UBlueprint* SubtoolBlueprint)
	{
		int Idx = Subtools.IndexOfByPredicate([&](UUserWidget* Subtool) {
			return Subtool->GetClass()->ClassGeneratedBy == SubtoolBlueprint;
			});

		if (Idx != -1) Subtools.RemoveAt(Idx);

		if (IsValid(ActiveSubtool) && SubtoolBlueprint == ActiveSubtool->GetClass()->ClassGeneratedBy)
		{
			ClearActiveSubtool();
			OnSubtoolChanged();
		}
	}
	
	void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override
	{
		if (PropertyChangedEvent.GetPropertyName().IsEqual("Subtool"))
		{
			SetActiveSubtool(Subtool);
			OnSubtoolChanged();
		}
	}

	void AddToEditorLevelViewport()
	{
		if (TSharedPtr<SLevelViewport> ActiveViewport = GetActiveViewport())
		{
			ActiveViewport->AddOverlayWidget(ActiveSubtool->TakeWidget());
		}
	}

	void RemoveFromEditorLevelViewport()
	{
		if (TSharedPtr<SLevelViewport> ActiveViewport = GetActiveViewport())
		{
			ActiveViewport->RemoveOverlayWidget(ActiveSubtool->TakeWidget());
		}
	}

	TSharedPtr<SLevelViewport> GetActiveViewport()
	{
		FLevelEditorModule& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
		TWeakPtr<ILevelEditor> Editor = LevelEditor.GetLevelEditorInstance();
		TSharedPtr<ILevelEditor> PinnedEditor(Editor.Pin());
		if (PinnedEditor.IsValid())
		{
			return PinnedEditor->GetActiveViewportInterface();
		}
		return nullptr;
	}

	void GetBlueprintOwnCategoryNames(const UClass* Class, TArray<FName>& Categories, bool bOnlyChildCategories=true)
	{
		TSet<FName> ParentPropertyNames;
		const UClass* Super = Class->GetSuperClass();
		while (Super)
		{
			for (TFieldIterator<const FProperty> It(Super, EFieldIteratorFlags::IncludeSuper); It; ++It)
			{
				ParentPropertyNames.Add((*It)->GetFName());
			}
			Super = Super->GetSuperClass();
		}

		for (TFieldIterator<const FProperty> It(Class, EFieldIteratorFlags::IncludeSuper); It; ++It)
		{
			const FProperty* Prop = *It;
			if (!ParentPropertyNames.Contains(Prop->GetFName()))
			{
				FString CategoryMeta = Prop->GetMetaData(TEXT("Category"));
				FName Cat = CategoryMeta.IsEmpty() ? FName(TEXT("Default")) : FName(*CategoryMeta);
				Categories.AddUnique(Cat);
			}
		}
	}


};