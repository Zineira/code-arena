#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArenaBoard.generated.h"

class AArenaFighter;
class ACameraActor;
class APlayerController;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UStaticMeshComponent;
class UTextRenderComponent;

/** What the board needs to know about a fighter to draw it (decoupled from the simulation). */
struct FArenaFighterView
{
	int32 X = 0;
	int32 Y = 0;
	int32 Facing = 1; // 0 North, 1 East, 2 South, 3 West
	int32 Hp = 0;
	int32 MaxHp = 0;
	FString Action;
	bool bBlocking = false;
	bool bCharging = false;
	bool bChanneling = false;
};

/** Draws the grid, the two fighters, effects and owns the gameplay camera. */
UCLASS()
class CODEARENA_API AArenaBoard : public AActor
{
	GENERATED_BODY()

public:
	AArenaBoard();

	virtual void Tick(float DeltaSeconds) override;

	void Build(int32 InWidth, int32 InHeight);
	void SetupFighters(const FString& PlayerName, const FString& EnemyName, int32 X0, int32 Y0, int32 X1, int32 Y1);
	void ApplyState(int32 Index, const FArenaFighterView& View);
	void PlayAttack(int32 Index);
	void PlayHit(int32 Index);
	void SetDefeated(int32 Index, bool bDefeated);
	void ShowPopup(int32 FighterIndex, const FString& Text, const FLinearColor& Color);
	void PlayFireball(int32 FromIndex, int32 ToX, int32 ToY);
	void ActivateCamera(APlayerController* PC);

	FVector GridToWorld(int32 X, int32 Y) const;
	static float DirToYaw(int32 Dir);

	/** World size of one grid cell. */
	UPROPERTY(EditAnywhere, Category = "Arena")
	float TileSize = 110.f;

	UPROPERTY(EditAnywhere, Category = "Arena|Camera")
	float CameraPitch = -58.f;

	UPROPERTY(EditAnywhere, Category = "Arena|Camera")
	float CameraDistanceScale = 2.45f;

	/** Shifts the arena to the right side of the screen so the code editor does not cover it. */
	UPROPERTY(EditAnywhere, Category = "Arena|Camera")
	float CameraSideOffsetScale = 0.42f;

	UPROPERTY(EditAnywhere, Category = "Arena|Camera")
	float CameraFOV = 55.f;

protected:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UInstancedStaticMeshComponent> TilesLight;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UInstancedStaticMeshComponent> TilesDark;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UInstancedStaticMeshComponent> Walls;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Projectile;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AArenaFighter>> Fighters;

	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> Camera;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextRenderComponent>> Popups;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> BaseMaterial;

private:
	void ColorComponent(UPrimitiveComponent* Component, const FLinearColor& Color);
	void SpawnFightersIfNeeded();

	int32 Width = 7;
	int32 Height = 7;
	TArray<float> PopupAges;
	FVector FireballStart = FVector::ZeroVector;
	FVector FireballEnd = FVector::ZeroVector;
	float FireballAlpha = 1.f;
};
