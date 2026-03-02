// Copyright WorldBLD LLC. All rights reserved.

#include "WorldBLDKitEditorUtils.h"
#include "WorldBLDEditorToolkit.h"
#include "HAL/PlatformApplicationMisc.h"
#include "EditorUtilityWidget.h"
#include "Blueprint/WidgetTree.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsSystemIncludes.h"
#pragma pack (push,8)
#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows/HideWindowsPlatformTypes.h"
#pragma pack (pop)
#endif

#define LOCTEXT_NAMESPACE "WorldBLDEditorToolkit"

///////////////////////////////////////////////////////////////////////////////////////////////////

#if PLATFORM_WINDOWS

static FString Platform_GetHwid()
{
	FString Id = FString(TEXT("(unknown)"));
#if 0 // NOTE: We could use this, but it may change when docking your laptop
	HW_PROFILE_INFO hwProfileInfo;
	if (GetCurrentHwProfile(&hwProfileInfo))
	{
		Id = hwProfileInfo.szHwProfileGuid;
	}
#else // This appears to be something the user can access with needing to run a script
	TArray<TCHAR> Output;
	Output.AddZeroed(128);
	DWORD OutBytes = static_cast<DWORD>(Output.Num());
	if (RegGetValue(HKEY_LOCAL_MACHINE, TEXT("SOFTWARE\\Microsoft\\SQMClient"), TEXT("MachineID"), RRF_RT_REG_SZ, nullptr,
		Output.GetData(), &OutBytes) == ERROR_SUCCESS)
	{
		Id = FString(OutBytes, Output.GetData());
	}
#endif

	return Id;
}
#else // Unsupported platform
static FString Platform_GetHwid()
{
	return FString(TEXT("(unsupported)"));
}
#endif

FString UWorldBLDKitEditorUtils::GetPlatformUserUniqueId()
{
	return Internal_GetPlatformUserUniqueId();
}

EWorldBLDLicenseState UWorldBLDKitEditorUtils::CheckLicenseStatus()
{
	// NOTE: Deprecated legacy HWID/file-based license check. Online licensing is handled by
	// `UWorldBLDAuthSubsystem` and related systems.
	return EWorldBLDLicenseState::Licensed;
}

FString UWorldBLDKitEditorUtils::Internal_GetPlatformUserUniqueId()
{
	FString HWID = FPlatformMisc::GetDeviceId();
	if (HWID.IsEmpty())
	{
		HWID = Platform_GetHwid();
	}

	if (HWID.IsEmpty())
	{
		HWID = TEXT("(unknown)");
	}

	HWID.ReplaceInline(TEXT("{"), TEXT(""));
	HWID.ReplaceInline(TEXT("}"), TEXT(""));
	
	return HWID;
}

void UWorldBLDKitEditorUtils::CopyTextToClipboard(FString Text)
{
	FPlatformApplicationMisc::ClipboardCopy(*Text);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

const UObject* UWorldBLDKitEditorUtils::GetStyleObjectByName(const UObject* InObj, const FName& Name)
{
	if (IsValid(InObj))
	{
		if (FProperty* Prop = InObj->GetClass()->FindPropertyByName(Name))
		{
			if (FObjectProperty* ObjProp = CastField<FObjectProperty>(Prop))
			{
				UObject* Value = ObjProp->GetObjectPropertyValue_InContainer(InObj);
				return Value;
			}
		}
	}
	return nullptr;
}

void UWorldBLDKitEditorUtils::GetAllWidgetsWithInterface(UWidget* Parent, TSubclassOf<UInterface> Interface, TArray<UWidget*>& FoundWidgets)
{
	if (!Interface || !IsValid(Parent))
	{
		return;
	}

	UWidget* Root = Parent;
	if (UEditorUtilityWidget* EUW = Cast<UEditorUtilityWidget>(Parent))
	{
		Root = EUW->WidgetTree->RootWidget;
	}
	if (!Root)
	{
		return;
	}

	if (UPanelWidget* PanelWidget = Cast<UPanelWidget>(Root))
	{
		for (UWidget* Child : PanelWidget->GetAllChildren())
		{
			if (Child->GetClass()->ImplementsInterface(Interface))
			{
				FoundWidgets.Push(Child);
			}
			GetAllWidgetsWithInterface(Child, Interface, FoundWidgets);
		}
	}
}

#undef LOCTEXT_NAMESPACE