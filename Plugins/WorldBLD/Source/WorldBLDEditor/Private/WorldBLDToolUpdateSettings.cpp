// Copyright WorldBLD LLC. All rights reserved.

#include "WorldBLDToolUpdateSettings.h"

FString UWorldBLDToolUpdateSettings::GetInstalledSha1(const FString& ToolId) const
{
	if (ToolId.IsEmpty())
	{
		return FString();
	}

	if (const FString* Found = InstalledToolSha1.Find(ToolId))
	{
		return *Found;
	}

	return FString();
}

void UWorldBLDToolUpdateSettings::SetInstalledSha1AndSave(const FString& ToolId, const FString& Sha1)
{
	if (ToolId.IsEmpty())
	{
		return;
	}

	if (Sha1.IsEmpty())
	{
		InstalledToolSha1.Remove(ToolId);
	}
	else
	{
		InstalledToolSha1.Add(ToolId, Sha1);
	}

	SaveConfig();
}


