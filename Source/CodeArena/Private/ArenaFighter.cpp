#include "ArenaFighter.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace ArenaFighterLocal
{
	UStaticMeshComponent* MakeShape(AActor* Owner, USceneComponent* Parent, const TCHAR* Name, UStaticMesh* Mesh, const FVector& Location, const FVector& Scale)
	{
		UStaticMeshComponent* Comp = Owner->CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Comp->SetupAttachment(Parent);
		Comp->SetStaticMesh(Mesh);
		Comp->SetRelativeLocation(Location);
		Comp->SetRelativeScale3D(Scale);
		Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Comp->SetCanEverAffectNavigation(false);
		return Comp;
	}
}

AArenaFighter::AArenaFighter()
{
	PrimaryActorTick.bCanEverTick = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	// Everything that animates (lunge / fall) hangs from Pivot so the root stays on the grid.
	Pivot = CreateDefaultSubobject<USceneComponent>(TEXT("Pivot"));
	Pivot->SetupAttachment(Root);

	using ArenaFighterLocal::MakeShape;
	Body = MakeShape(this, Pivot, TEXT("Body"), CylinderMesh.Object, FVector(0, 0, 45), FVector(0.55f, 0.55f, 0.9f));
	Head = MakeShape(this, Pivot, TEXT("Head"), SphereMesh.Object, FVector(0, 0, 112), FVector(0.45f));
	Weapon = MakeShape(this, Pivot, TEXT("Weapon"), CubeMesh.Object, FVector(45, 22, 60), FVector(0.6f, 0.08f, 0.08f));
	Shield = MakeShape(this, Pivot, TEXT("Shield"), CubeMesh.Object, FVector(38, 0, 60), FVector(0.08f, 0.7f, 0.8f));
	Shield->SetVisibility(false);

	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(Root);
	Label->SetRelativeLocation(FVector(0, 0, 175));
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetVerticalAlignment(EVRTA_TextCenter);
	Label->SetWorldSize(22.f);
	Label->SetTextRenderColor(FColor::White);
}

void AArenaFighter::SetTeamColor(const FLinearColor& InColor)
{
	TeamColor = InColor;
	if (!BodyMaterial && Body->GetMaterial(0))
	{
		BodyMaterial = UMaterialInstanceDynamic::Create(Body->GetMaterial(0), this);
		Body->SetMaterial(0, BodyMaterial);
		Head->SetMaterial(0, BodyMaterial);
	}
	if (!AccentMaterial && Weapon->GetMaterial(0))
	{
		AccentMaterial = UMaterialInstanceDynamic::Create(Weapon->GetMaterial(0), this);
		Weapon->SetMaterial(0, AccentMaterial);
		Shield->SetMaterial(0, AccentMaterial);
	}
	if (AccentMaterial)
	{
		AccentMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.85f, 0.85f, 0.9f));
	}
	UpdateColor();
}

void AArenaFighter::SetName(const FString& InName)
{
	DisplayName = InName;
}

void AArenaFighter::TeleportTo(const FVector& Location, float Yaw)
{
	TargetLocation = Location;
	TargetYaw = Yaw;
	SetActorLocationAndRotation(Location, FRotator(0.f, Yaw, 0.f));
}

void AArenaFighter::MoveTo(const FVector& Location, float Yaw)
{
	TargetLocation = Location;
	TargetYaw = Yaw;
}

void AArenaFighter::SetStatus(int32 Hp, int32 MaxHp, const FString& ActionName, bool bBlocking, bool bInCharging, bool bInChanneling)
{
	Label->SetText(FText::FromString(FString::Printf(TEXT("%s  %d/%d\n%s"), *DisplayName, FMath::Max(Hp, 0), MaxHp, *ActionName)));
	Shield->SetVisibility(bBlocking);
	bCharging = bInCharging;
	bChanneling = bInChanneling;
}

void AArenaFighter::PlayLunge()
{
	LungeTime = 0.f;
}

void AArenaFighter::PlayHitFlash()
{
	FlashAlpha = 1.f;
}

void AArenaFighter::SetDefeated(bool bInDefeated)
{
	bDefeated = bInDefeated;
	if (!bDefeated)
	{
		DefeatAlpha = 0.f;
		Pivot->SetRelativeRotation(FRotator::ZeroRotator);
	}
}

FVector AArenaFighter::GetPopupAnchor() const
{
	return GetActorLocation() + FVector(0, 0, 140);
}

void AArenaFighter::UpdateColor()
{
	if (!BodyMaterial)
	{
		return;
	}
	FLinearColor Color = TeamColor;
	if (bCharging)
	{
		Color = FMath::Lerp(Color, FLinearColor(1.f, 0.45f, 0.f), 0.5f + 0.5f * FMath::Sin(PulseTime * 18.f));
	}
	else if (bChanneling)
	{
		Color = FMath::Lerp(Color, FLinearColor(0.2f, 1.f, 0.3f), 0.5f + 0.5f * FMath::Sin(PulseTime * 12.f));
	}
	if (bDefeated)
	{
		Color = FMath::Lerp(Color, FLinearColor(0.1f, 0.1f, 0.1f), DefeatAlpha);
	}
	Color = FMath::Lerp(Color, FLinearColor::White, FlashAlpha);
	BodyMaterial->SetVectorParameterValue(TEXT("Color"), Color);
}

void AArenaFighter::FaceLabelToCamera()
{
	const UWorld* World = GetWorld();
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (PC && PC->PlayerCameraManager)
	{
		const FVector ToCamera = PC->PlayerCameraManager->GetCameraLocation() - Label->GetComponentLocation();
		Label->SetWorldRotation(ToCamera.Rotation());
	}
}

void AArenaFighter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	PulseTime += DeltaSeconds;
	FlashAlpha = FMath::Max(0.f, FlashAlpha - DeltaSeconds * 4.f);
	if (bDefeated)
	{
		DefeatAlpha = FMath::Min(1.f, DefeatAlpha + DeltaSeconds * 2.5f);
	}

	const FVector NewLocation = FMath::VInterpTo(GetActorLocation(), TargetLocation, DeltaSeconds, 12.f);
	const FRotator NewRotation = FMath::RInterpTo(GetActorRotation(), FRotator(0.f, TargetYaw, 0.f), DeltaSeconds, 14.f);
	SetActorLocationAndRotation(NewLocation, NewRotation);

	// Lunge forward and back over 0.25s.
	LungeTime = FMath::Min(1.f, LungeTime + DeltaSeconds * 4.f);
	const float Lunge = FMath::Sin(LungeTime * UE_PI) * 35.f;
	Pivot->SetRelativeLocation(FVector(Lunge, 0.f, 0.f));
	Pivot->SetRelativeRotation(FRotator(0.f, 0.f, -85.f * DefeatAlpha));

	const float Pulse = (bCharging || bChanneling) ? 1.f + 0.06f * FMath::Sin(PulseTime * 20.f) : 1.f;
	Body->SetRelativeScale3D(FVector(0.55f * Pulse, 0.55f * Pulse, 0.9f));

	UpdateColor();
	FaceLabelToCamera();
}
