#include "ArenaBoard.h"

#include "ArenaFighter.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace ArenaBoardLocal
{
	const float BoardZ = 20.f;
	const float PopupLifetime = 1.1f;
	const FLinearColor PlayerColor(0.08f, 0.35f, 1.f);
	const FLinearColor EnemyColor(1.f, 0.12f, 0.08f);
}

AArenaBoard::AArenaBoard()
{
	PrimaryActorTick.bCanEverTick = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterial> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	BaseMaterial = ShapeMaterial.Object;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	UStaticMesh* Cube = CubeMesh.Object;
	const auto MakeInstanced = [this, Cube](const TCHAR* Name)
	{
		UInstancedStaticMeshComponent* Comp = CreateDefaultSubobject<UInstancedStaticMeshComponent>(Name);
		Comp->SetupAttachment(Root);
		Comp->SetStaticMesh(Cube);
		Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Comp->SetCanEverAffectNavigation(false);
		return Comp;
	};
	TilesLight = MakeInstanced(TEXT("TilesLight"));
	TilesDark = MakeInstanced(TEXT("TilesDark"));
	Walls = MakeInstanced(TEXT("Walls"));

	Projectile = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Projectile"));
	Projectile->SetupAttachment(Root);
	Projectile->SetStaticMesh(SphereMesh.Object);
	Projectile->SetRelativeScale3D(FVector(0.35f));
	Projectile->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Projectile->SetVisibility(false);
}

void AArenaBoard::ColorComponent(UPrimitiveComponent* Component, const FLinearColor& Color)
{
	if (!BaseMaterial)
	{
		return;
	}
	UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(BaseMaterial, this);
	Material->SetVectorParameterValue(TEXT("Color"), Color);
	Component->SetMaterial(0, Material);
}

FVector AArenaBoard::GridToWorld(int32 X, int32 Y) const
{
	// Grid North (+y) is world +X (away from the camera), grid East (+x) is world +Y (screen right).
	const float OffsetX = (Height - 1) * 0.5f * TileSize;
	const float OffsetY = (Width - 1) * 0.5f * TileSize;
	return GetActorLocation() + FVector(Y * TileSize - OffsetX, X * TileSize - OffsetY, ArenaBoardLocal::BoardZ + 10.f);
}

float AArenaBoard::DirToYaw(int32 Dir)
{
	switch (Dir)
	{
	case 0: return 0.f;    // North
	case 1: return 90.f;   // East
	case 2: return 180.f;  // South
	default: return -90.f; // West
	}
}

void AArenaBoard::Build(int32 InWidth, int32 InHeight)
{
	Width = FMath::Max(1, InWidth);
	Height = FMath::Max(1, InHeight);

	TilesLight->ClearInstances();
	TilesDark->ClearInstances();
	Walls->ClearInstances();

	const float Scale = TileSize / 100.f; // the engine cube is 100 units wide
	for (int32 X = 0; X < Width; ++X)
	{
		for (int32 Y = 0; Y < Height; ++Y)
		{
			const FVector Center = GridToWorld(X, Y) - GetActorLocation() - FVector(0, 0, 10.f);
			const FTransform Transform(FRotator::ZeroRotator, Center, FVector(Scale * 0.96f, Scale * 0.96f, 0.1f));
			((X + Y) % 2 == 0 ? TilesLight : TilesDark)->AddInstance(Transform);
		}
	}

	// Low walls around the arena.
	const float HalfX = Height * 0.5f * TileSize;
	const float HalfY = Width * 0.5f * TileSize;
	const float WallThickness = 0.25f;
	const float WallHeight = 0.35f;
	const float Z = ArenaBoardLocal::BoardZ + 12.f;
	Walls->AddInstance(FTransform(FRotator::ZeroRotator, FVector(HalfX + 12.f, 0, Z), FVector(WallThickness, (HalfY * 2.f + 50.f) / 100.f, WallHeight)));
	Walls->AddInstance(FTransform(FRotator::ZeroRotator, FVector(-HalfX - 12.f, 0, Z), FVector(WallThickness, (HalfY * 2.f + 50.f) / 100.f, WallHeight)));
	Walls->AddInstance(FTransform(FRotator::ZeroRotator, FVector(0, HalfY + 12.f, Z), FVector((HalfX * 2.f) / 100.f, WallThickness, WallHeight)));
	Walls->AddInstance(FTransform(FRotator::ZeroRotator, FVector(0, -HalfY - 12.f, Z), FVector((HalfX * 2.f) / 100.f, WallThickness, WallHeight)));

	ColorComponent(TilesLight, FLinearColor(0.32f, 0.34f, 0.38f));
	ColorComponent(TilesDark, FLinearColor(0.16f, 0.17f, 0.2f));
	ColorComponent(Walls, FLinearColor(0.05f, 0.05f, 0.07f));
	ColorComponent(Projectile, FLinearColor(1.f, 0.4f, 0.02f));

	SpawnFightersIfNeeded();
}

void AArenaBoard::SpawnFightersIfNeeded()
{
	if (Fighters.Num() == 2 || !GetWorld())
	{
		return;
	}
	for (int32 i = 0; i < 2; ++i)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AArenaFighter* Fighter = GetWorld()->SpawnActor<AArenaFighter>(AArenaFighter::StaticClass(), GridToWorld(0, 0), FRotator::ZeroRotator, Params);
		Fighter->SetTeamColor(i == 0 ? ArenaBoardLocal::PlayerColor : ArenaBoardLocal::EnemyColor);
		Fighters.Add(Fighter);
	}
}

void AArenaBoard::SetupFighters(const FString& PlayerName, const FString& EnemyName, int32 X0, int32 Y0, int32 X1, int32 Y1)
{
	SpawnFightersIfNeeded();
	if (Fighters.Num() != 2)
	{
		return;
	}
	Fighters[0]->SetName(PlayerName);
	Fighters[1]->SetName(EnemyName);
	Fighters[0]->TeleportTo(GridToWorld(X0, Y0), DirToYaw(1));
	Fighters[1]->TeleportTo(GridToWorld(X1, Y1), DirToYaw(3));
	for (AArenaFighter* Fighter : Fighters)
	{
		Fighter->SetDefeated(false);
	}
	FireballAlpha = 1.f;
	Projectile->SetVisibility(false);
}

void AArenaBoard::ApplyState(int32 Index, const FArenaFighterView& View)
{
	if (!Fighters.IsValidIndex(Index))
	{
		return;
	}
	AArenaFighter* Fighter = Fighters[Index];
	Fighter->MoveTo(GridToWorld(View.X, View.Y), DirToYaw(View.Facing));
	Fighter->SetStatus(View.Hp, View.MaxHp, View.Action, View.bBlocking, View.bCharging, View.bChanneling);
}

void AArenaBoard::PlayAttack(int32 Index)
{
	if (Fighters.IsValidIndex(Index))
	{
		Fighters[Index]->PlayLunge();
	}
}

void AArenaBoard::PlayHit(int32 Index)
{
	if (Fighters.IsValidIndex(Index))
	{
		Fighters[Index]->PlayHitFlash();
	}
}

void AArenaBoard::SetDefeated(int32 Index, bool bDefeated)
{
	if (Fighters.IsValidIndex(Index))
	{
		Fighters[Index]->SetDefeated(bDefeated);
	}
}

void AArenaBoard::ShowPopup(int32 FighterIndex, const FString& Text, const FLinearColor& Color)
{
	if (!Fighters.IsValidIndex(FighterIndex))
	{
		return;
	}
	UTextRenderComponent* Popup = NewObject<UTextRenderComponent>(this);
	Popup->SetupAttachment(Root);
	Popup->RegisterComponent();
	Popup->SetHorizontalAlignment(EHTA_Center);
	Popup->SetVerticalAlignment(EVRTA_TextCenter);
	Popup->SetWorldSize(34.f);
	Popup->SetText(FText::FromString(Text));
	Popup->SetTextRenderColor(Color.ToFColor(true));
	// Offset a little when several popups stack on the same fighter.
	Popup->SetWorldLocation(Fighters[FighterIndex]->GetPopupAnchor() + FVector(0, 0, 25.f * (Popups.Num() % 3)));
	Popups.Add(Popup);
	PopupAges.Add(0.f);
}

void AArenaBoard::PlayFireball(int32 FromIndex, int32 ToX, int32 ToY)
{
	if (!Fighters.IsValidIndex(FromIndex))
	{
		return;
	}
	FireballStart = Fighters[FromIndex]->GetActorLocation() + FVector(0, 0, 60.f);
	FireballEnd = GridToWorld(ToX, ToY) + FVector(0, 0, 60.f);
	FireballAlpha = 0.f;
	Projectile->SetWorldLocation(FireballStart);
	Projectile->SetVisibility(true);
}

void AArenaBoard::ActivateCamera(APlayerController* PC)
{
	if (!PC || !GetWorld())
	{
		return;
	}
	if (!Camera)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Camera = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity, Params);
	}

	const float Extent = FMath::Max(Width, Height) * TileSize;
	const float Distance = Extent * CameraDistanceScale;
	const float PitchRad = FMath::DegreesToRadians(-CameraPitch);
	const FVector Center = GetActorLocation();
	const FVector Location = Center + FVector(-Distance * FMath::Cos(PitchRad), -Extent * CameraSideOffsetScale, Distance * FMath::Sin(PitchRad));
	Camera->SetActorLocationAndRotation(Location, FRotator(CameraPitch, 0.f, 0.f));
	Camera->GetCameraComponent()->SetFieldOfView(CameraFOV);
	Camera->GetCameraComponent()->SetConstraintAspectRatio(false);

	PC->bAutoManageActiveCameraTarget = false;
	PC->SetViewTarget(Camera);
}

void AArenaBoard::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (FireballAlpha < 1.f)
	{
		FireballAlpha = FMath::Min(1.f, FireballAlpha + DeltaSeconds * 3.5f);
		Projectile->SetWorldLocation(FMath::Lerp(FireballStart, FireballEnd, FireballAlpha));
		if (FireballAlpha >= 1.f)
		{
			Projectile->SetVisibility(false);
		}
	}

	FVector CameraLocation = FVector::ZeroVector;
	const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	const bool bHasCamera = PC && PC->PlayerCameraManager;
	if (bHasCamera)
	{
		CameraLocation = PC->PlayerCameraManager->GetCameraLocation();
	}

	for (int32 i = Popups.Num() - 1; i >= 0; --i)
	{
		UTextRenderComponent* Popup = Popups[i];
		PopupAges[i] += DeltaSeconds;
		if (!Popup || PopupAges[i] >= ArenaBoardLocal::PopupLifetime)
		{
			if (Popup)
			{
				Popup->DestroyComponent();
			}
			Popups.RemoveAt(i);
			PopupAges.RemoveAt(i);
			continue;
		}
		Popup->AddWorldOffset(FVector(0, 0, 70.f * DeltaSeconds));
		if (bHasCamera)
		{
			Popup->SetWorldRotation((CameraLocation - Popup->GetComponentLocation()).Rotation());
		}
	}
}
