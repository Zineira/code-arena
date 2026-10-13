#pragma once

#include "CoreMinimal.h"

/** Player-facing content (texts in Portuguese, shop catalog, starter code). */
namespace CodeArenaContent
{
	struct FLevelText
	{
		FText Title;
		FText Description;
	};

	struct FUnlockDef
	{
		FString Id;
		FText Name;
		FText Description;
		int32 Cost = 0;
		FString Requires; // another unlock id that must be bought first (empty = none)
	};

	FLevelText GetLevelText(int32 LevelIndex);
	const TArray<FUnlockDef>& GetUnlocks();
	FString GetStarterCode();
	FText GetHelpText();

	/** Operations per tick granted by the "cpu" upgrades. */
	int32 GetOpsPerTick(const TArray<FString>& Unlocks);
	/** Max HP granted by the "armor" upgrades. */
	int32 GetMaxHp(const TArray<FString>& Unlocks);
}
