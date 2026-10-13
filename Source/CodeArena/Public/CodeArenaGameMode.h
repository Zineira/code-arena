#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "CodeArenaGameMode.generated.h"

class AArenaBoard;
class SCodeArenaHUD;
class UCodeArenaSaveGame;
struct FCodeArenaRuntime;

/** Fighter info shown by the HUD. */
struct FCodeArenaFighterInfo
{
	FString Name;
	int32 Hp = 0;
	int32 MaxHp = 0;
	int32 Energy = 0;
	int32 MaxEnergy = 100;
	FString Action;
};

/**
 * Owns the whole minigame: progression (gold, unlocks, levels), the running match
 * (FightScript simulation), the 3D board and the Slate HUD.
 */
UCLASS()
class CODEARENA_API ACodeArenaGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ACodeArenaGameMode();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	/** No pawn: the player only writes code. */
	virtual void RestartPlayer(AController* NewPlayer) override {}

	// --- Match ---------------------------------------------------------
	void StartMatch();
	void StopMatch();
	bool IsMatchRunning() const { return bRunning; }
	int32 GetMatchTick() const;
	int32 GetMaxTicks() const;
	/** Line the player's script is currently executing (0 if none). */
	int32 GetPlayerLine() const;
	bool GetFighterInfo(int32 Index, FCodeArenaFighterInfo& OutInfo) const;

	// --- Speed ---------------------------------------------------------
	void CycleSpeed();
	FText GetSpeedText() const;

	// --- Levels --------------------------------------------------------
	int32 GetNumLevels() const;
	int32 GetSelectedLevel() const;
	bool IsLevelAvailable(int32 LevelIndex) const;
	bool IsLevelBeaten(int32 LevelIndex) const;
	void SelectLevel(int32 LevelIndex);

	// --- Code ----------------------------------------------------------
	FString GetCode() const;
	void SetCode(const FString& Code);
	void ResetCodeToStarter();

	// --- Progression ---------------------------------------------------
	int32 GetGold() const;
	int32 GetOpsPerTick() const;
	bool HasUnlock(const FString& Id) const;
	bool CanBuy(const FString& Id) const;
	bool Buy(const FString& Id);
	void ResetProgress();

	/** Result of the last match ("VITORIA!", ...), empty while fighting. */
	const FText& GetBannerText() const { return BannerText; }
	const FLinearColor& GetBannerColor() const { return BannerColor; }

	/** Tick interval of the simulation at 1x speed (seconds). */
	UPROPERTY(EditAnywhere, Category = "Code Arena")
	float BaseTickInterval = 0.45f;

protected:
	UPROPERTY(Transient)
	TObjectPtr<AArenaBoard> Board;

	UPROPERTY(Transient)
	TObjectPtr<UCodeArenaSaveGame> Progress;

private:
	void InitPlayerView();
	void EnsureLighting();
	void LoadProgress();
	void SaveProgress();
	void PrepareBoardForLevel();
	void StepMatch();
	void ProcessEvents();
	void SyncBoard();
	void EndMatch(int32 Winner, const FString& Reason);
	void Log(const FString& Text, const FLinearColor& Color);

	TSharedPtr<FCodeArenaRuntime> Runtime;
	TSharedPtr<SCodeArenaHUD> Hud;
	bool bRunning = false;
	float TickAccumulator = 0.f;
	int32 SpeedIndex = 0;
	int32 InitAttempts = 0;
	FText BannerText;
	FLinearColor BannerColor = FLinearColor::White;
};
