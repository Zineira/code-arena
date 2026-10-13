#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArenaFighter.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;
class UMaterialInstanceDynamic;

/** Visual representation of a fighter built from engine basic shapes. Purely cosmetic. */
UCLASS()
class CODEARENA_API AArenaFighter : public AActor
{
	GENERATED_BODY()

public:
	AArenaFighter();

	virtual void Tick(float DeltaSeconds) override;

	void SetTeamColor(const FLinearColor& InColor);
	void SetName(const FString& InName);

	/** Snaps to a location/yaw (used when a match is (re)started). */
	void SnapTo(const FVector& Location, float Yaw);
	/** Smoothly moves to a location/yaw. */
	void MoveTo(const FVector& Location, float Yaw);

	void SetStatus(int32 Hp, int32 MaxHp, const FString& ActionName, bool bBlocking, bool bCharging, bool bChanneling);
	void PlayLunge();
	void PlayHitFlash();
	void SetDefeated(bool bInDefeated);

	/** Point above the head, used for popups. */
	FVector GetPopupAnchor() const;

protected:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Pivot;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Head;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Weapon;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Shield;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UTextRenderComponent> Label;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BodyMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> AccentMaterial;

private:
	void UpdateColor();
	void FaceLabelToCamera();

	FLinearColor TeamColor = FLinearColor::Blue;
	FString DisplayName;
	FVector TargetLocation = FVector::ZeroVector;
	float TargetYaw = 0.f;
	float LungeTime = 1.f;
	float FlashAlpha = 0.f;
	float PulseTime = 0.f;
	bool bCharging = false;
	bool bChanneling = false;
	bool bDefeated = false;
	float DefeatAlpha = 0.f;
};
