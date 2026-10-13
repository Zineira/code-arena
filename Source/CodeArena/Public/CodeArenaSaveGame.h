#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "CodeArenaSaveGame.generated.h"

/** Player progression: gold, purchased unlocks, beaten levels and the code written for each level. */
UCLASS()
class CODEARENA_API UCodeArenaSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	int32 Gold = 0;

	UPROPERTY()
	TArray<FString> Unlocks;

	/** Number of campaign levels beaten (levels [0, LevelsBeaten] are playable). */
	UPROPERTY()
	int32 LevelsBeaten = 0;

	UPROPERTY()
	int32 SelectedLevel = 0;

	/** Source code per level index. */
	UPROPERTY()
	TArray<FString> LevelCode;
};
