#include "CodeArenaGameMode.h"

#include "ArenaBoard.h"
#include "CodeArena.h"
#include "CodeArenaContent.h"
#include "CodeArenaSaveGame.h"
#include "UI/SCodeArenaHUD.h"

#include "FightLevels.h"
#include "FightSim.h"

#include "Components/LightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

#define LOCTEXT_NAMESPACE "CodeArena"

/** Non-UObject state of a running match (kept out of the header). */
struct FCodeArenaRuntime
{
	FightSim::FMatch Match;
};

namespace CodeArenaGameModeLocal
{
	const TCHAR* SaveSlot = TEXT("CodeArena");
	const float SpeedFactors[] = { 1.f, 2.f, 4.f, 10.f };
	const int32 NumSpeeds = UE_ARRAY_COUNT(SpeedFactors);

	const FLinearColor ColorInfo(0.75f, 0.8f, 0.9f);
	const FLinearColor ColorPrint(1.f, 1.f, 1.f);
	const FLinearColor ColorEnemy(1.f, 0.6f, 0.55f);
	const FLinearColor ColorError(1.f, 0.3f, 0.25f);
	const FLinearColor ColorGood(0.35f, 1.f, 0.45f);
	const FLinearColor ColorWarn(1.f, 0.85f, 0.3f);

	std::string ToStd(const FString& Text)
	{
		const FTCHARToUTF8 Converter(*Text);
		return std::string(Converter.Get(), static_cast<size_t>(Converter.Length()));
	}

	FString FromStd(const std::string& Text)
	{
		return FString(UTF8_TO_TCHAR(Text.c_str()));
	}

	FString ActionLabel(FightSim::EAction Action)
	{
		switch (Action)
		{
		case FightSim::EAction::Idle: return TEXT("parado");
		case FightSim::EAction::Think: return TEXT("a pensar...");
		case FightSim::EAction::Wait: return TEXT("descansa");
		case FightSim::EAction::Move: return TEXT("anda");
		case FightSim::EAction::Attack: return TEXT("ataca");
		case FightSim::EAction::Block: return TEXT("bloqueia");
		case FightSim::EAction::Dash: return TEXT("investida");
		case FightSim::EAction::Charge: return TEXT("a carregar");
		case FightSim::EAction::Heavy: return TEXT("golpe pesado");
		case FightSim::EAction::Channel: return TEXT("a concentrar");
		case FightSim::EAction::Heal: return TEXT("cura");
		case FightSim::EAction::Fireball: return TEXT("bola de fogo");
		case FightSim::EAction::Tired: return TEXT("cansado");
		}
		return FString();
	}

	FString ReasonLabel(const FString& Reason)
	{
		if (Reason == TEXT("knockout")) return TEXT("KO");
		if (Reason == TEXT("time limit")) return TEXT("tempo esgotado");
		if (Reason == TEXT("script stopped")) return TEXT("um programa parou");
		if (Reason == TEXT("both scripts stopped")) return TEXT("os dois programas pararam");
		return Reason;
	}
}

ACodeArenaGameMode::ACodeArenaGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	DefaultPawnClass = nullptr;
}

void ACodeArenaGameMode::BeginPlay()
{
	Super::BeginPlay();

	LoadProgress();
	EnsureLighting();

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Board = GetWorld()->SpawnActor<AArenaBoard>(AArenaBoard::StaticClass(), FTransform::Identity, Params);
	PrepareBoardForLevel();

	InitPlayerView();
}

void ACodeArenaGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Hud.IsValid())
	{
		if (UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr)
		{
			Viewport->RemoveViewportWidgetContent(Hud.ToSharedRef());
		}
		Hud.Reset();
	}
	SaveProgress();
	Super::EndPlay(EndPlayReason);
}

void ACodeArenaGameMode::InitPlayerView()
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	UGameViewportClient* Viewport = GetWorld()->GetGameViewport();
	if (!PC || !Viewport)
	{
		// The local player may not exist yet on the very first frame.
		if (++InitAttempts < 120)
		{
			GetWorldTimerManager().SetTimerForNextTick(this, &ACodeArenaGameMode::InitPlayerView);
		}
		return;
	}

	Board->ActivateCamera(PC);

	Hud = SNew(SCodeArenaHUD).GameMode(this);
	Viewport->AddViewportWidgetContent(Hud.ToSharedRef(), 10);

	PC->SetShowMouseCursor(true);
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(Hud->GetEditorWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PC->SetInputMode(InputMode);

	Log(TEXT("Bem-vindo à Code Arena! Escreve o programa do teu lutador e carrega em Executar."), CodeArenaGameModeLocal::ColorInfo);
	Log(TEXT("Abre a Ajuda para veres todas as funções disponíveis."), CodeArenaGameModeLocal::ColorInfo);
}

void ACodeArenaGameMode::EnsureLighting()
{
	if (TActorIterator<ADirectionalLight>(GetWorld()))
	{
		return; // the level already has lighting
	}

	const auto SpawnLight = [this](const FRotator& Rotation, float Intensity, bool bShadows)
	{
		ADirectionalLight* Light = GetWorld()->SpawnActor<ADirectionalLight>(ADirectionalLight::StaticClass(), FVector(0, 0, 500), Rotation);
		if (Light && Light->GetLightComponent())
		{
			Light->GetLightComponent()->SetMobility(EComponentMobility::Movable);
			Light->GetLightComponent()->SetIntensity(Intensity);
			Light->GetLightComponent()->SetCastShadows(bShadows);
		}
	};
	SpawnLight(FRotator(-55.f, 35.f, 0.f), 6.f, true);
	SpawnLight(FRotator(-30.f, -150.f, 0.f), 2.f, false);
}

void ACodeArenaGameMode::LoadProgress()
{
	using namespace CodeArenaGameModeLocal;
	if (UGameplayStatics::DoesSaveGameExist(SaveSlot, 0))
	{
		Progress = Cast<UCodeArenaSaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlot, 0));
	}
	if (!Progress)
	{
		Progress = Cast<UCodeArenaSaveGame>(UGameplayStatics::CreateSaveGameObject(UCodeArenaSaveGame::StaticClass()));
	}

	const int32 NumLevels = GetNumLevels();
	while (Progress->LevelCode.Num() < NumLevels)
	{
		Progress->LevelCode.Add(FString());
	}
	for (FString& Code : Progress->LevelCode)
	{
		if (Code.IsEmpty())
		{
			Code = CodeArenaContent::GetStarterCode();
		}
	}
	Progress->LevelsBeaten = FMath::Clamp(Progress->LevelsBeaten, 0, NumLevels);
	Progress->SelectedLevel = FMath::Clamp(Progress->SelectedLevel, 0, FMath::Min(Progress->LevelsBeaten, NumLevels - 1));
}

void ACodeArenaGameMode::SaveProgress()
{
	if (Progress)
	{
		UGameplayStatics::SaveGameToSlot(Progress, CodeArenaGameModeLocal::SaveSlot, 0);
	}
}

void ACodeArenaGameMode::PrepareBoardForLevel()
{
	if (!Board || !Progress)
	{
		return;
	}
	const FightLevels::FLevelDef& Level = FightLevels::GetLevels()[static_cast<size_t>(GetSelectedLevel())];
	const FightSim::FMatchConfig Config = FightLevels::MakeMatchConfig(Level, FightSim::FFighterConfig(), 1);
	const FString EnemyName = CodeArenaContent::GetLevelText(GetSelectedLevel()).Title.ToString();

	Board->Build(Level.Width, Level.Height);
	Board->SetupFighters(TEXT("Tu"), EnemyName, Config.StartPositions[0], Config.StartPositions[1], Config.StartPositions[2], Config.StartPositions[3]);

	for (int32 i = 0; i < 2; ++i)
	{
		FCodeArenaFighterInfo Info;
		GetFighterInfo(i, Info);
		FArenaFighterView View;
		View.X = Config.StartPositions[static_cast<size_t>(i * 2)];
		View.Y = Config.StartPositions[static_cast<size_t>(i * 2 + 1)];
		View.Facing = i == 0 ? 1 : 3;
		View.Hp = Info.Hp;
		View.MaxHp = Info.MaxHp;
		Board->ApplyState(i, View);
	}
}

void ACodeArenaGameMode::StartMatch()
{
	using namespace CodeArenaGameModeLocal;
	if (!Progress || !Board)
	{
		return;
	}

	const int32 LevelIndex = GetSelectedLevel();
	const FightLevels::FLevelDef& Level = FightLevels::GetLevels()[static_cast<size_t>(LevelIndex)];
	const FString EnemyName = CodeArenaContent::GetLevelText(LevelIndex).Title.ToString();

	FightSim::FFighterConfig Player;
	Player.Name = "Player";
	Player.Script = ToStd(GetCode());
	Player.OpsPerTick = CodeArenaContent::GetOpsPerTick(Progress->Unlocks);
	Player.MaxHp = CodeArenaContent::GetMaxHp(Progress->Unlocks);
	for (const FString& Unlock : Progress->Unlocks)
	{
		Player.Unlocks.insert(ToStd(Unlock));
	}

	const FightSim::FMatchConfig Config = FightLevels::MakeMatchConfig(Level, Player, static_cast<uint32_t>(FMath::Rand()) | 1u);

	bRunning = false;
	BannerText = FText::GetEmpty();
	Runtime = MakeShared<FCodeArenaRuntime>();

	int ErrorFighter = 0;
	int ErrorLine = 0;
	std::string ErrorMessage;
	if (!Runtime->Match.Init(Config, ErrorFighter, ErrorLine, ErrorMessage))
	{
		Runtime.Reset();
		if (ErrorFighter == 0)
		{
			Log(FString::Printf(TEXT("Erro de compilação na linha %d: %s"), ErrorLine, *FromStd(ErrorMessage)), ColorError);
		}
		else
		{
			Log(FString::Printf(TEXT("O script do inimigo não compila (linha %d): %s"), ErrorLine, *FromStd(ErrorMessage)), ColorError);
		}
		BannerText = LOCTEXT("CompileError", "ERRO NO CÓDIGO");
		BannerColor = ColorError;
		return;
	}

	SaveProgress();
	Board->SetupFighters(TEXT("Tu"), EnemyName, Config.StartPositions[0], Config.StartPositions[1], Config.StartPositions[2], Config.StartPositions[3]);
	SyncBoard();

	bRunning = true;
	TickAccumulator = 0.f;
	Log(FString::Printf(TEXT("--- Combate contra %s (%d operações por turno) ---"), *EnemyName, Player.OpsPerTick), ColorInfo);
}

void ACodeArenaGameMode::StopMatch()
{
	if (bRunning)
	{
		bRunning = false;
		Log(TEXT("Combate interrompido."), CodeArenaGameModeLocal::ColorWarn);
	}
}

void ACodeArenaGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bRunning || !Runtime.IsValid())
	{
		return;
	}

	const float Interval = BaseTickInterval / CodeArenaGameModeLocal::SpeedFactors[SpeedIndex];
	TickAccumulator += DeltaSeconds;
	int32 Steps = 0;
	while (bRunning && TickAccumulator >= Interval && Steps < 20)
	{
		TickAccumulator -= Interval;
		StepMatch();
		++Steps;
	}
}

void ACodeArenaGameMode::StepMatch()
{
	Runtime->Match.Step();
	SyncBoard();
	ProcessEvents();
}

void ACodeArenaGameMode::ProcessEvents()
{
	using namespace CodeArenaGameModeLocal;
	using FightSim::EEventType;

	const std::vector<FightSim::FEvent> Events = Runtime->Match.ConsumeEvents();
	for (const FightSim::FEvent& Event : Events)
	{
		const int32 Who = Event.Fighter;
		const FString Text = FromStd(Event.Text);
		switch (Event.Type)
		{
		case EEventType::Print:
			Log(Who == 0 ? Text : TEXT("[inimigo] ") + Text, Who == 0 ? ColorPrint : ColorEnemy);
			break;
		case EEventType::Error:
			if (Who == 0)
			{
				Log(FString::Printf(TEXT("Erro na linha %d: %s"), Event.Amount, *Text), ColorError);
			}
			else
			{
				Log(FString::Printf(TEXT("O inimigo teve um erro na linha %d: %s"), Event.Amount, *Text), ColorEnemy);
			}
			Board->ShowPopup(Who, TEXT("ERRO"), ColorError);
			break;
		case EEventType::Hit:
			Board->PlayAttack(Who);
			Board->PlayHit(Event.Target);
			Board->ShowPopup(Event.Target, FString::Printf(TEXT("-%d"), Event.Amount), FLinearColor(1.f, 0.45f, 0.1f));
			break;
		case EEventType::Blocked:
			Board->PlayAttack(Who);
			Board->ShowPopup(Event.Target, Event.Amount > 0 ? FString::Printf(TEXT("escudo -%d"), Event.Amount) : FString(TEXT("bloqueado!")), FLinearColor(0.3f, 0.8f, 1.f));
			break;
		case EEventType::Missed:
			Board->PlayAttack(Who);
			Board->ShowPopup(Who, TEXT("falhou"), FLinearColor(0.7f, 0.7f, 0.7f));
			break;
		case EEventType::Healed:
			Board->ShowPopup(Who, FString::Printf(TEXT("+%d"), Event.Amount), ColorGood);
			break;
		case EEventType::Fireball:
			Board->PlayFireball(Who, Event.X, Event.Y);
			if (Event.Target >= 0)
			{
				Board->PlayHit(Event.Target);
				Board->ShowPopup(Event.Target, FString::Printf(TEXT("-%d"), Event.Amount), FLinearColor(1.f, 0.45f, 0.1f));
			}
			break;
		case EEventType::Charging:
			Board->ShowPopup(Who, Text == TEXT("charge") ? TEXT("a carregar...") : TEXT("a concentrar..."), ColorWarn);
			break;
		case EEventType::Tired:
			Board->ShowPopup(Who, TEXT("cansado!"), ColorWarn);
			break;
		case EEventType::ScriptEnded:
			if (Who == 0)
			{
				Log(TEXT("O teu programa terminou e o lutador ficou parado. Dica: usa while True: para repetir."), ColorWarn);
			}
			break;
		case EEventType::MatchOver:
			EndMatch(Event.Target, Text);
			break;
		default:
			break;
		}
	}
}

void ACodeArenaGameMode::SyncBoard()
{
	if (!Runtime.IsValid() || !Board)
	{
		return;
	}
	for (int32 i = 0; i < 2; ++i)
	{
		const FightSim::FFighter& Fighter = Runtime->Match.GetFighter(i);
		FArenaFighterView View;
		View.X = Fighter.X;
		View.Y = Fighter.Y;
		View.Facing = static_cast<int32>(Fighter.Facing);
		View.Hp = Fighter.Hp;
		View.MaxHp = Fighter.MaxHp;
		View.Action = Runtime->Match.GetTick() > 0 ? CodeArenaGameModeLocal::ActionLabel(Fighter.LastAction) : FString();
		View.bBlocking = Fighter.bBlocking;
		View.bCharging = Fighter.LastAction == FightSim::EAction::Charge;
		View.bChanneling = Fighter.LastAction == FightSim::EAction::Channel;
		Board->ApplyState(i, View);
		Board->SetDefeated(i, Fighter.Hp <= 0);
	}
}

void ACodeArenaGameMode::EndMatch(int32 Winner, const FString& Reason)
{
	using namespace CodeArenaGameModeLocal;
	bRunning = false;

	const int32 LevelIndex = GetSelectedLevel();
	const FightLevels::FLevelDef& Level = FightLevels::GetLevels()[static_cast<size_t>(LevelIndex)];
	const FString ReasonText = ReasonLabel(Reason);
	const int32 Ticks = GetMatchTick();

	if (Winner == 0)
	{
		const bool bFirstWin = !IsLevelBeaten(LevelIndex);
		const int32 Reward = bFirstWin ? Level.Reward : Level.RepeatReward;
		Progress->Gold += Reward;
		if (bFirstWin)
		{
			Progress->LevelsBeaten = FMath::Max(Progress->LevelsBeaten, LevelIndex + 1);
		}
		BannerText = FText::Format(LOCTEXT("Victory", "VITÓRIA!  +{0} ouro"), FText::AsNumber(Reward));
		BannerColor = ColorGood;
		Log(FString::Printf(TEXT("VITÓRIA (%s) em %d turnos! +%d ouro."), *ReasonText, Ticks, Reward), ColorGood);
		if (bFirstWin && LevelIndex + 1 < GetNumLevels())
		{
			Log(FString::Printf(TEXT("Novo adversário desbloqueado: %s"), *CodeArenaContent::GetLevelText(LevelIndex + 1).Title.ToString()), ColorGood);
		}
	}
	else if (Winner == 1)
	{
		BannerText = LOCTEXT("Defeat", "DERROTA");
		BannerColor = ColorError;
		Log(FString::Printf(TEXT("DERROTA (%s) ao fim de %d turnos. Ajusta o código e tenta outra vez!"), *ReasonText, Ticks), ColorError);
	}
	else
	{
		BannerText = LOCTEXT("Draw", "EMPATE");
		BannerColor = ColorWarn;
		Log(FString::Printf(TEXT("EMPATE (%s)."), *ReasonText), ColorWarn);
	}
	SaveProgress();
}

void ACodeArenaGameMode::Log(const FString& Text, const FLinearColor& Color)
{
	UE_LOG(LogCodeArena, Log, TEXT("%s"), *Text);
	if (Hud.IsValid())
	{
		Hud->AddConsoleLine(Text, Color);
	}
}

int32 ACodeArenaGameMode::GetMatchTick() const
{
	return Runtime.IsValid() ? Runtime->Match.GetTick() : 0;
}

int32 ACodeArenaGameMode::GetMaxTicks() const
{
	return FightLevels::GetLevels()[static_cast<size_t>(GetSelectedLevel())].MaxTicks;
}

int32 ACodeArenaGameMode::GetPlayerLine() const
{
	return Runtime.IsValid() && bRunning ? Runtime->Match.GetCurrentLine(0) : 0;
}

bool ACodeArenaGameMode::GetFighterInfo(int32 Index, FCodeArenaFighterInfo& OutInfo) const
{
	if (!Progress || Index < 0 || Index > 1)
	{
		return false;
	}
	const int32 LevelIndex = GetSelectedLevel();
	const FightLevels::FLevelDef& Level = FightLevels::GetLevels()[static_cast<size_t>(LevelIndex)];
	OutInfo.Name = Index == 0 ? TEXT("Tu") : CodeArenaContent::GetLevelText(LevelIndex).Title.ToString();

	if (Runtime.IsValid())
	{
		const FightSim::FFighter& Fighter = Runtime->Match.GetFighter(Index);
		OutInfo.Hp = FMath::Max(Fighter.Hp, 0);
		OutInfo.MaxHp = Fighter.MaxHp;
		OutInfo.Energy = Fighter.Energy;
		OutInfo.MaxEnergy = Runtime->Match.GetConfig().Rules.MaxEnergy;
		OutInfo.Action = Runtime->Match.GetTick() > 0 ? CodeArenaGameModeLocal::ActionLabel(Fighter.LastAction) : FString();
		return true;
	}

	OutInfo.MaxHp = Index == 0 ? CodeArenaContent::GetMaxHp(Progress->Unlocks) : Level.EnemyHp;
	OutInfo.Hp = OutInfo.MaxHp;
	OutInfo.Energy = 100;
	OutInfo.MaxEnergy = 100;
	return true;
}

void ACodeArenaGameMode::CycleSpeed()
{
	SpeedIndex = (SpeedIndex + 1) % CodeArenaGameModeLocal::NumSpeeds;
}

FText ACodeArenaGameMode::GetSpeedText() const
{
	return FText::Format(LOCTEXT("Speed", "Velocidade {0}x"), FText::AsNumber(FMath::RoundToInt(CodeArenaGameModeLocal::SpeedFactors[SpeedIndex])));
}

int32 ACodeArenaGameMode::GetNumLevels() const
{
	return static_cast<int32>(FightLevels::GetLevels().size());
}

int32 ACodeArenaGameMode::GetSelectedLevel() const
{
	return Progress ? FMath::Clamp(Progress->SelectedLevel, 0, GetNumLevels() - 1) : 0;
}

bool ACodeArenaGameMode::IsLevelAvailable(int32 LevelIndex) const
{
	return Progress && LevelIndex >= 0 && LevelIndex < GetNumLevels() && LevelIndex <= Progress->LevelsBeaten;
}

bool ACodeArenaGameMode::IsLevelBeaten(int32 LevelIndex) const
{
	return Progress && LevelIndex >= 0 && LevelIndex < Progress->LevelsBeaten;
}

void ACodeArenaGameMode::SelectLevel(int32 LevelIndex)
{
	if (!IsLevelAvailable(LevelIndex) || LevelIndex == GetSelectedLevel())
	{
		return;
	}
	StopMatch();
	Runtime.Reset();
	BannerText = FText::GetEmpty();
	Progress->SelectedLevel = LevelIndex;
	PrepareBoardForLevel();
	SaveProgress();
	if (Hud.IsValid())
	{
		Hud->ReloadCode();
	}
}

FString ACodeArenaGameMode::GetCode() const
{
	return Progress && Progress->LevelCode.IsValidIndex(GetSelectedLevel()) ? Progress->LevelCode[GetSelectedLevel()] : FString();
}

void ACodeArenaGameMode::SetCode(const FString& Code)
{
	if (Progress && Progress->LevelCode.IsValidIndex(GetSelectedLevel()))
	{
		Progress->LevelCode[GetSelectedLevel()] = Code;
	}
}

void ACodeArenaGameMode::ResetCodeToStarter()
{
	SetCode(CodeArenaContent::GetStarterCode());
	if (Hud.IsValid())
	{
		Hud->ReloadCode();
	}
}

int32 ACodeArenaGameMode::GetGold() const
{
	return Progress ? Progress->Gold : 0;
}

int32 ACodeArenaGameMode::GetOpsPerTick() const
{
	return Progress ? CodeArenaContent::GetOpsPerTick(Progress->Unlocks) : 100;
}

bool ACodeArenaGameMode::HasUnlock(const FString& Id) const
{
	return Progress && Progress->Unlocks.Contains(Id);
}

bool ACodeArenaGameMode::CanBuy(const FString& Id) const
{
	if (!Progress || HasUnlock(Id))
	{
		return false;
	}
	for (const CodeArenaContent::FUnlockDef& Def : CodeArenaContent::GetUnlocks())
	{
		if (Def.Id == Id)
		{
			return Progress->Gold >= Def.Cost && (Def.Requires.IsEmpty() || HasUnlock(Def.Requires));
		}
	}
	return false;
}

bool ACodeArenaGameMode::Buy(const FString& Id)
{
	if (!CanBuy(Id))
	{
		return false;
	}
	for (const CodeArenaContent::FUnlockDef& Def : CodeArenaContent::GetUnlocks())
	{
		if (Def.Id == Id)
		{
			Progress->Gold -= Def.Cost;
			Progress->Unlocks.Add(Id);
			SaveProgress();
			Log(FString::Printf(TEXT("Desbloqueado: %s"), *Def.Name.ToString()), CodeArenaGameModeLocal::ColorGood);
			if (!bRunning)
			{
				PrepareBoardForLevel(); // max HP may have changed
			}
			return true;
		}
	}
	return false;
}

void ACodeArenaGameMode::ResetProgress()
{
	StopMatch();
	Runtime.Reset();
	BannerText = FText::GetEmpty();
	Progress = Cast<UCodeArenaSaveGame>(UGameplayStatics::CreateSaveGameObject(UCodeArenaSaveGame::StaticClass()));
	for (int32 i = 0; i < GetNumLevels(); ++i)
	{
		Progress->LevelCode.Add(CodeArenaContent::GetStarterCode());
	}
	SaveProgress();
	PrepareBoardForLevel();
	if (Hud.IsValid())
	{
		Hud->ReloadCode();
	}
	Log(TEXT("Progresso apagado."), CodeArenaGameModeLocal::ColorWarn);
}

#undef LOCTEXT_NAMESPACE
