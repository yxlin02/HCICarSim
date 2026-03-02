// Copyright WorldBLD LLC. All rights reserved.

#include "CallInEditorAssetActions.h"

#include "ContentBrowserDelegates.h" // from ContentBrowser Editor module
#include "ContentBrowserModule.h" // from ContentBrowser Editor module
#include "BlueprintEditorModule.h" // from Kismet
#include "IStructureDetailsView.h" // from PropertyEditor
#include "Kismet2/BlueprintEditorUtils.h" // from UnrealEd
#include "Widgets/Layout/SScrollBox.h"

// SEE: Engine\Source\Editor\Blutility\Private\BlutilityContentBrowserExtensions.h

#define LOCTEXT_NAMESPACE "CallInEditorAssetActions"

static FContentBrowserMenuExtender_SelectedAssets ContentBrowserExtenderDelegate;
static FDelegateHandle ContentBrowserExtenderDelegateHandle;

///////////////////////////////////////////////////////////////////////////////////////////////////

class FCallInEditorAssetActions_Impl
{
public:
	static TSharedRef<FExtender> OnExtendContentBrowserAssetSelectionMenu(const TArray<FAssetData>& SelectedAssets);
	static TArray<FContentBrowserMenuExtender_SelectedAssets>& GetExtenderDelegates();
	static void CreateEditorActionsMenu(FMenuBuilder& MenuBuilder, TArray<UObject*> Objects);
};

void FCallInEditorAssetActions::InstallHooks()
{
    ContentBrowserExtenderDelegate = FContentBrowserMenuExtender_SelectedAssets::CreateStatic(&FCallInEditorAssetActions_Impl::OnExtendContentBrowserAssetSelectionMenu);

	TArray<FContentBrowserMenuExtender_SelectedAssets>& CBMenuExtenderDelegates = FCallInEditorAssetActions_Impl::GetExtenderDelegates();
	CBMenuExtenderDelegates.Add(ContentBrowserExtenderDelegate);
	ContentBrowserExtenderDelegateHandle = CBMenuExtenderDelegates.Last().GetHandle();
}

void FCallInEditorAssetActions::RemoveHooks()
{
	TArray<FContentBrowserMenuExtender_SelectedAssets>& CBMenuExtenderDelegates = FCallInEditorAssetActions_Impl::GetExtenderDelegates();
	CBMenuExtenderDelegates.RemoveAll([](const FContentBrowserMenuExtender_SelectedAssets& Delegate){ return Delegate.GetHandle() == ContentBrowserExtenderDelegateHandle; });
}

/** Dialog widget used to display function properties */
class SCustomFunctionParamDialog : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SCustomFunctionParamDialog) {}

	/** Text to display on the "OK" button */
	SLATE_ARGUMENT(FText, OkButtonText)

	/** Tooltip text for the "OK" button */
	SLATE_ARGUMENT(FText, OkButtonTooltipText)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TWeakPtr<SWindow> InParentWindow, TSharedRef<FStructOnScope> InStructOnScope);

	bool bOKPressed;
};

///////////////////////////////////////////////////////////////////////////////////////////////////

TSharedRef<FExtender> FCallInEditorAssetActions_Impl::OnExtendContentBrowserAssetSelectionMenu(const TArray<FAssetData>& SelectedAssets)
{
	TSharedRef<FExtender> Extender(new FExtender());

	TArray<UObject*> AssetList;
	for (const FAssetData& AssetData : SelectedAssets)
	{
		UObject* Asset = AssetData.GetAsset();
		if (UBlueprint* Blueprint = Cast<UBlueprint>(Asset))
		{
			if (IsValid(Blueprint->GeneratedClass))
			{
				AssetList.AddUnique(Blueprint->GeneratedClass->GetDefaultObject());
			}
			else
			{
				AssetList.AddUnique(Blueprint);
			}
		}
		else
		{
			AssetList.AddUnique(Asset);
		}
	}

	if (AssetList.Num() > 0)
	{
		// Add asset actions extender
		Extender->AddMenuExtension(
			"CommonAssetActions",
			EExtensionHook::After,
			nullptr,
			FMenuExtensionDelegate::CreateStatic(&FCallInEditorAssetActions_Impl::CreateEditorActionsMenu, AssetList));
	}

	return Extender;
}

TArray<FContentBrowserMenuExtender_SelectedAssets>& FCallInEditorAssetActions_Impl::GetExtenderDelegates()
{
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
	return ContentBrowserModule.GetAllAssetViewContextMenuExtenders();
}

void FCallInEditorAssetActions_Impl::CreateEditorActionsMenu(FMenuBuilder& MenuBuilder, TArray<UObject*> Objects)
{
	const static FName NAME_CallInEditor(TEXT("CallInEditor"));

	// Helper struct to track the util to call a function on
	struct FFunctionAndUtil
	{
		FFunctionAndUtil(UFunction* InFunction, UObject* InObject)
			: Function(InFunction)
			, Object(InObject) {}

		bool operator==(const FFunctionAndUtil& InFunction) const
		{
			return InFunction.Function == Function;
		}

		UFunction* Function;
		UObject* Object;
	};
	TMap<UFunction*, TArray<UObject*>> FunctionsMap;
	TSet<UClass*> ProcessedClasses;

	// Find the exposed functions available in each class, making sure to not list shared functions from a parent class more than once
	for (UObject* Object : Objects)
	{
		if (!IsValid(Object) || !IsValid(Object->GetClass()))
		{
			continue;
		}

		UClass* Class = Object->GetClass();

		if (ProcessedClasses.Contains(Class))
		{
			continue;
		}

		for (UClass* ParentClass = Class; ParentClass != UObject::StaticClass(); ParentClass = ParentClass->GetSuperClass())
		{
			ProcessedClasses.Add(ParentClass);
		}

		for (TFieldIterator<UFunction> FunctionIt(Class); FunctionIt; ++FunctionIt)
		{
			if (UFunction* Func = *FunctionIt)
			{
				if (Func->HasMetaData(NAME_CallInEditor) && Func->GetReturnProperty() == nullptr)
				{
					if (const FString* CategoryMeta = Func->FindMetaData("Category"))
					{
						if (!(*CategoryMeta).Contains(TEXT("AssetAction")))
						{
							continue;
						}
					}

					FunctionsMap.FindOrAdd(Func, {}).AddUnique(Object);
				}
			}
		}
	}

	// Sort the functions by name
	TArray<UFunction*> FunctionList;
	FunctionsMap.GetKeys(FunctionList);
	FunctionList.Sort([](const UFunction& A, const UFunction& B) { return A.GetName() < B.GetName(); });

	// Add a menu item for each function
	if (FunctionList.Num() > 0)
	{
		MenuBuilder.AddSubMenu(
			LOCTEXT("CustomEditorAssetActions", "Call In Editor Actions"), 
			LOCTEXT("CustomEditorAssetActionsTooltip", "Actions available for the selected actors based on exposed CallInEditor functions"),
			FNewMenuDelegate::CreateLambda([FunctionList, FunctionsMap](FMenuBuilder& InMenuBuilder)
			{
				for (UFunction* Function : FunctionList)
				{
					const FText TooltipText = FText::Format(LOCTEXT("AssetUtilTooltipFormat", "{0}\n(Shift-click to edit script)"), Function->GetToolTipText());

					InMenuBuilder.AddMenuEntry(
						Function->GetDisplayNameText(), 
						TooltipText,
						FSlateIcon("EditorStyle", "GraphEditor.Event_16x"),
						FExecuteAction::CreateLambda([Function, FunctionList, FunctionsMap] 
						{
							if (FSlateApplication::Get().GetModifierKeys().IsShiftDown())
							{
								// Edit the script if we have shift held down
								const FFunctionAndUtil FunctionAndUtil(Function, FunctionsMap[Function][0]);
								if (UBlueprint* Blueprint = Cast<UBlueprint>(Cast<UObject>(FunctionAndUtil.Object)->GetClass()->ClassGeneratedBy))
								{
									if (IAssetEditorInstance* AssetEditor = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->FindEditorForAsset(Blueprint, true))
									{
										check(AssetEditor->GetEditorName() == TEXT("BlueprintEditor"));
										IBlueprintEditor* BlueprintEditor = static_cast<IBlueprintEditor*>(AssetEditor);
										BlueprintEditor->JumpToHyperlink(FunctionAndUtil.Function, false);
									}
									else
									{
										FBlueprintEditorModule& BlueprintEditorModule = FModuleManager::LoadModuleChecked<FBlueprintEditorModule>("Kismet");
										const TSharedRef<IBlueprintEditor> BlueprintEditor = BlueprintEditorModule.CreateBlueprintEditor(EToolkitMode::Standalone, TSharedPtr<IToolkitHost>(), Blueprint, false);
										BlueprintEditor->JumpToHyperlink(FunctionAndUtil.Function, false);
									}
								}
							}
							else
							{
								// NOTE: We want to specifically operate on the CDO / specific data of the asset depending on the context.
								for (UObject* FinalObject : FunctionsMap[Function])
								{
									FFunctionAndUtil FunctionAndUtil(Function, FinalObject);
									const bool bAlreadyDirty = FinalObject->GetPackage()->IsDirty();
									auto TryDirtyObject = [=]()
										{
											const bool bIsDirty = FinalObject->GetPackage()->IsDirty();
											if (bIsDirty != bAlreadyDirty)
											{
												if (UBlueprint* Blueprint = UBlueprint::GetBlueprintFromClass(FinalObject->GetClass()))
												{
													FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
													// Call PostEditChange() on any Actors that might be based on this Blueprint
													FBlueprintEditorUtils::PostEditChangeBlueprintActors(Blueprint);
												}
											}
										};

									if (FunctionAndUtil.Function->NumParms > 0)
									{
										// Create a parameter struct and fill in defaults
										TSharedRef<FStructOnScope> FuncParams = MakeShared<FStructOnScope>(FunctionAndUtil.Function);
										for (TFieldIterator<FProperty> It(FunctionAndUtil.Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
										{
											FString Defaults;
											if (UEdGraphSchema_K2::FindFunctionParameterDefaultValue(FunctionAndUtil.Function, *It, Defaults))
											{
												It->ImportText_Direct(*Defaults, It->ContainerPtrToValuePtr<uint8>(FuncParams->GetStructMemory()), nullptr, PPF_None);
											}
										}

										// pop up a dialog to input params to the function
										TSharedRef<SWindow> Window = SNew(SWindow)
											.Title(FunctionAndUtil.Function->GetDisplayNameText())
											.ClientSize(FVector2D(400, 200))
											.SupportsMinimize(false)
											.SupportsMaximize(false);

										TSharedPtr<SCustomFunctionParamDialog> Dialog;
										Window->SetContent(
											SAssignNew(Dialog, SCustomFunctionParamDialog, Window, FuncParams)
											.OkButtonText(LOCTEXT("OKButton", "OK"))
											.OkButtonTooltipText(FunctionAndUtil.Function->GetToolTipText()));

										GEditor->EditorAddModalWindow(Window);

										if (Dialog->bOKPressed)
										{
											FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "BlutilityAction", "Blutility Action"));
											FEditorScriptExecutionGuard ScriptGuard;
											FinalObject->ProcessEvent(FunctionAndUtil.Function, FuncParams->GetStructMemory());
											TryDirtyObject();
										}
									}
									else
									{
										FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "BlutilityAction", "Blutility Action"));
										FEditorScriptExecutionGuard ScriptGuard;
										FinalObject->ProcessEvent(FunctionAndUtil.Function, nullptr);
										TryDirtyObject();
									}
								}
							}
						}));
				}
			}),
			false,
			FSlateIcon("EditorStyle", "GraphEditor.Event_16x"));
	}
}

void SCustomFunctionParamDialog::Construct(const FArguments& InArgs, TWeakPtr<SWindow> InParentWindow, TSharedRef<FStructOnScope> InStructOnScope)
{
	bOKPressed = false;

	// Initialize details view
	FDetailsViewArgs DetailsViewArgs;
	{
		DetailsViewArgs.bAllowSearch = false;
		DetailsViewArgs.bHideSelectionTip = true;
		DetailsViewArgs.bLockable = false;
		DetailsViewArgs.bSearchInitialKeyFocus = true;
		DetailsViewArgs.bUpdatesFromSelection = false;
		DetailsViewArgs.bShowOptions = false;
		DetailsViewArgs.bShowModifiedPropertiesOption = false;
		DetailsViewArgs.bShowObjectLabel = false;
		DetailsViewArgs.bForceHiddenPropertyVisibility = true;
		DetailsViewArgs.bShowScrollBar = false;
	}
	
	FStructureDetailsViewArgs StructureViewArgs;
	{
		StructureViewArgs.bShowObjects = true;
		StructureViewArgs.bShowAssets = true;
		StructureViewArgs.bShowClasses = true;
		StructureViewArgs.bShowInterfaces = true;
	}

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::Get().LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	const TSharedRef<IStructureDetailsView> StructureDetailsView = PropertyEditorModule.CreateStructureDetailView(DetailsViewArgs, StructureViewArgs, InStructOnScope);

	StructureDetailsView->GetDetailsView()->SetIsPropertyVisibleDelegate(FIsPropertyVisible::CreateLambda([](const FPropertyAndParent& InPropertyAndParent)
	{
		return InPropertyAndParent.Property.HasAnyPropertyFlags(CPF_Parm);
	}));

	StructureDetailsView->GetDetailsView()->ForceRefresh();

	ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SNew(SScrollBox)
			+SScrollBox::Slot()
			[
				StructureDetailsView->GetWidget().ToSharedRef()
			]
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Right)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.Padding(2.0f)
				.AutoWidth()
				[
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), "FlatButton.Success")
					.ForegroundColor(FLinearColor::White)
					.ContentPadding(FMargin(6, 2))
					.OnClicked_Lambda([this, InParentWindow]()
					{
						if(InParentWindow.IsValid())
						{
							InParentWindow.Pin()->RequestDestroyWindow();
						}
						bOKPressed = true;
						return FReply::Handled(); 
					})
					.ToolTipText(InArgs._OkButtonTooltipText)
					[
						SNew(STextBlock)
						.TextStyle(FAppStyle::Get(), "ContentBrowser.TopBar.Font")
						.Text(InArgs._OkButtonText)
					]
				]
				+SHorizontalBox::Slot()
				.Padding(2.0f)
				.AutoWidth()
				[
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), "FlatButton")
					.ForegroundColor(FLinearColor::White)
					.ContentPadding(FMargin(6, 2))
					.OnClicked_Lambda([InParentWindow]()
					{ 
						if(InParentWindow.IsValid())
						{
							InParentWindow.Pin()->RequestDestroyWindow();
						}
						return FReply::Handled(); 
					})
					[
						SNew(STextBlock)
						.TextStyle(FAppStyle::Get(), "ContentBrowser.TopBar.Font")
						.Text(LOCTEXT("Cancel", "Cancel"))
					]
				]
			]
		]
	];
}

///////////////////////////////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
