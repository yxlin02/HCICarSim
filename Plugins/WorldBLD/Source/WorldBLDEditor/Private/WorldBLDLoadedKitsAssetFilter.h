// Copyright WorldBLD LLC. All rights reserved.

#pragma once

#include "CoreMinimal.h"

namespace WorldBLD::Editor::LoadedKitsAssetFilter
{
	/**
	 * Build (and internally cache) a set of package names that are referenced (hard/soft/management) by the
	 * currently loaded WorldBLD kits/bundles in the active WorldBLD editor mode.
	 *
	 * Returns null if there is no active WorldBLD editor mode or no loaded kits/bundles.
	 */
	TSharedPtr<const TSet<FName>> GetAllowedPackageNamesFromLoadedKits();

	/** Force the internal cache to refresh on next query. */
	void InvalidateCache();
}


