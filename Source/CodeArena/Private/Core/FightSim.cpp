#include "FightSim.h"

#include <cstdlib>

namespace FightSim
{
	using FightScript::ECallResult;
	using FightScript::EVMStatus;
	using FightScript::FValue;

	const char* ActionName(EAction Action)
	{
		switch (Action)
		{
		case EAction::Idle: return "idle";
		case EAction::Think: return "think";
		case EAction::Wait: return "wait";
		case EAction::Move: return "move";
		case EAction::Attack: return "attack";
		case EAction::Block: return "block";
		case EAction::Dash: return "dash";
		case EAction::Charge: return "charge";
		case EAction::Heavy: return "heavy";
		case EAction::Channel: return "channel";
		case EAction::Heal: return "heal";
		case EAction::Fireball: return "fireball";
		case EAction::Tired: return "tired";
		}
		return "idle";
	}

	const char* DirName(EDir Dir)
	{
		switch (Dir)
		{
		case EDir::North: return "North";
		case EDir::East: return "East";
		case EDir::South: return "South";
		case EDir::West: return "West";
		}
		return "North";
	}

	static bool ParseDir(const FValue& Value, EDir& OutDir, std::string& OutError)
	{
		if (Value.Type == FightScript::EValueType::String)
		{
			for (int i = 0; i < 4; ++i)
			{
				const EDir Dir = static_cast<EDir>(i);
				if (Value.StringValue == DirName(Dir))
				{
					OutDir = Dir;
					return true;
				}
			}
		}
		OutError = "ValueError: expected a direction (North, East, South or West), got " + Value.ToDisplayString();
		return false;
	}

	static EDir OppositeDir(EDir Dir)
	{
		return static_cast<EDir>((static_cast<int>(Dir) + 2) % 4);
	}

	bool IsUnlocked(const FFighterConfig& Config, const std::string& Id)
	{
		return Config.bAllUnlocked || Config.Unlocks.count(Id) > 0;
	}

	void FMatch::AddEvent(EEventType Type, int Fighter, const std::string& Text, int Amount, int Target, int X, int Y)
	{
		FEvent Event;
		Event.Type = Type;
		Event.Fighter = Fighter;
		Event.Text = Text;
		Event.Amount = Amount;
		Event.Target = Target;
		Event.X = X;
		Event.Y = Y;
		Events.push_back(Event);
	}

	std::vector<FEvent> FMatch::ConsumeEvents()
	{
		std::vector<FEvent> Out;
		Out.swap(Events);
		return Out;
	}

	bool FMatch::InBounds(int X, int Y) const
	{
		return X >= 0 && Y >= 0 && X < Config.Width && Y < Config.Height;
	}

	void FMatch::DirDelta(EDir Dir, int& OutDx, int& OutDy)
	{
		OutDx = 0;
		OutDy = 0;
		switch (Dir)
		{
		case EDir::North: OutDy = 1; break;
		case EDir::East: OutDx = 1; break;
		case EDir::South: OutDy = -1; break;
		case EDir::West: OutDx = -1; break;
		}
	}

	EDir FMatch::DirTowards(int FromX, int FromY, int ToX, int ToY) const
	{
		const int Dx = ToX - FromX;
		const int Dy = ToY - FromY;
		if (std::abs(Dx) >= std::abs(Dy) && Dx != 0)
		{
			return Dx > 0 ? EDir::East : EDir::West;
		}
		return Dy >= 0 ? EDir::North : EDir::South;
	}

	int FMatch::Distance() const
	{
		return std::abs(Fighters[0].X - Fighters[1].X) + std::abs(Fighters[0].Y - Fighters[1].Y);
	}

	void FMatch::BindBuiltins(int Index)
	{
		const int Other = 1 - Index;
		FFighter& Me = Fighters[static_cast<size_t>(Index)];
		FightScript::FBuiltinTable& Table = Me.Builtins;

		FightScript::AddStandardLibrary(Table, [this, Index](const std::string& Line)
		{
			AddEvent(EEventType::Print, Index, Line);
		}, &RandomState);

		for (int i = 0; i < 4; ++i)
		{
			const char* Name = DirName(static_cast<EDir>(i));
			Table.AddConstant(Name, FValue::MakeString(Name));
		}

		const auto AddNumberSensor = [&Table](const char* Name, std::function<int()> Getter)
		{
			Table.AddFunction(Name, 0, 0, [Getter](const std::vector<FValue>&, FValue& OutResult, std::string&)
			{
				OutResult = FValue::MakeNumber(static_cast<double>(Getter()));
				return ECallResult::Ok;
			});
		};

		AddNumberSensor("my_x", [this, Index] { return Fighters[static_cast<size_t>(Index)].X; });
		AddNumberSensor("my_y", [this, Index] { return Fighters[static_cast<size_t>(Index)].Y; });
		AddNumberSensor("my_hp", [this, Index] { return Fighters[static_cast<size_t>(Index)].Hp; });
		AddNumberSensor("my_energy", [this, Index] { return Fighters[static_cast<size_t>(Index)].Energy; });
		AddNumberSensor("enemy_x", [this, Other] { return Fighters[static_cast<size_t>(Other)].X; });
		AddNumberSensor("enemy_y", [this, Other] { return Fighters[static_cast<size_t>(Other)].Y; });
		AddNumberSensor("enemy_hp", [this, Other] { return Fighters[static_cast<size_t>(Other)].Hp; });
		AddNumberSensor("enemy_energy", [this, Other] { return Fighters[static_cast<size_t>(Other)].Energy; });
		AddNumberSensor("distance", [this] { return Distance(); });
		AddNumberSensor("get_tick", [this] { return Tick; });
		AddNumberSensor("arena_width", [this] { return Config.Width; });
		AddNumberSensor("arena_height", [this] { return Config.Height; });

		Table.AddFunction("enemy_action", 0, 0, [this, Other](const std::vector<FValue>&, FValue& OutResult, std::string&)
		{
			OutResult = FValue::MakeString(ActionName(Fighters[static_cast<size_t>(Other)].LastAction));
			return ECallResult::Ok;
		});

		Table.AddFunction("dir_to_enemy", 0, 0, [this, Index, Other](const std::vector<FValue>&, FValue& OutResult, std::string&)
		{
			const FFighter& Self = Fighters[static_cast<size_t>(Index)];
			const FFighter& Enemy = Fighters[static_cast<size_t>(Other)];
			OutResult = FValue::MakeString(DirName(DirTowards(Self.X, Self.Y, Enemy.X, Enemy.Y)));
			return ECallResult::Ok;
		});

		Table.AddFunction("opposite", 1, 1, [](const std::vector<FValue>& Args, FValue& OutResult, std::string& OutError)
		{
			EDir Dir = EDir::North;
			if (!ParseDir(Args[0], Dir, OutError))
			{
				return ECallResult::Error;
			}
			OutResult = FValue::MakeString(DirName(OppositeDir(Dir)));
			return ECallResult::Ok;
		});

		Table.AddFunction("can_move", 1, 1, [this, Index, Other](const std::vector<FValue>& Args, FValue& OutResult, std::string& OutError)
		{
			EDir Dir = EDir::North;
			if (!ParseDir(Args[0], Dir, OutError))
			{
				return ECallResult::Error;
			}
			int Dx = 0;
			int Dy = 0;
			DirDelta(Dir, Dx, Dy);
			const FFighter& Self = Fighters[static_cast<size_t>(Index)];
			const FFighter& Enemy = Fighters[static_cast<size_t>(Other)];
			const int Nx = Self.X + Dx;
			const int Ny = Self.Y + Dy;
			OutResult = FValue::MakeBool(InBounds(Nx, Ny) && !(Nx == Enemy.X && Ny == Enemy.Y));
			return ECallResult::Ok;
		});

		const auto AddAction = [this, Index, &Table](const char* Name, EAction Action, bool bNeedsDir)
		{
			const int ArgCount = bNeedsDir ? 1 : 0;
			Table.AddFunction(Name, ArgCount, ArgCount, [this, Index, Action, bNeedsDir](const std::vector<FValue>& Args, FValue& OutResult, std::string& OutError)
			{
				FFighter& Self = Fighters[static_cast<size_t>(Index)];
				EDir Dir = Self.Facing;
				if (bNeedsDir && !ParseDir(Args[0], Dir, OutError))
				{
					return ECallResult::Error;
				}
				Self.Requested = Action;
				Self.RequestedDir = Dir;
				OutResult = FValue::MakeNone();
				return ECallResult::Suspend;
			});
		};

		AddAction("wait", EAction::Wait, false);
		AddAction("move", EAction::Move, true);
		AddAction("attack", EAction::Attack, false);
		AddAction("block", EAction::Block, false);
		AddAction("dash", EAction::Dash, true);
		AddAction("heavy_attack", EAction::Heavy, false);
		AddAction("heal", EAction::Heal, false);
		AddAction("fireball", EAction::Fireball, true);

		static const struct { const char* Builtin; const char* Unlock; } Gates[] = {
			{ "block", "block" },
			{ "dash", "dash" },
			{ "enemy_action", "scanner" },
			{ "enemy_energy", "scanner" },
			{ "dir_to_enemy", "radar" },
			{ "heavy_attack", "heavy" },
			{ "heal", "heal" },
			{ "fireball", "fireball" },
		};
		const FFighterConfig& FighterConfig = Config.Fighters[static_cast<size_t>(Index)];
		for (const auto& Gate : Gates)
		{
			if (!IsUnlocked(FighterConfig, Gate.Unlock))
			{
				Table.SetLocked(Gate.Builtin, true,
					std::string("Locked: ") + Gate.Builtin + "() is not unlocked yet (buy '" + Gate.Unlock + "' in the shop)");
			}
		}
	}

	bool FMatch::Init(const FMatchConfig& InConfig, int& OutErrorFighter, int& OutErrorLine, std::string& OutErrorMessage)
	{
		Config = InConfig;
		Events.clear();
		Tick = 0;
		bOver = false;
		Winner = -1;
		RandomState = Config.Seed != 0 ? Config.Seed : 1u;

		for (int i = 0; i < 2; ++i)
		{
			const FFighterConfig& FighterConfig = Config.Fighters[static_cast<size_t>(i)];
			FFighter& F = Fighters[static_cast<size_t>(i)];
			F = FFighter();
			F.Name = FighterConfig.Name;
			F.X = Config.StartPositions[static_cast<size_t>(i * 2)];
			F.Y = Config.StartPositions[static_cast<size_t>(i * 2 + 1)];
			F.MaxHp = FighterConfig.MaxHp;
			F.Hp = F.MaxHp;
			F.Energy = Config.Rules.MaxEnergy;
			F.OpsPerTick = FighterConfig.OpsPerTick;
			F.Facing = i == 0 ? EDir::East : EDir::West;
			BindBuiltins(i);

			FightScript::FCompileOptions Options;
			Options.bAllowFor = IsUnlocked(FighterConfig, "for");
			Options.bAllowDef = IsUnlocked(FighterConfig, "def");
			Options.bAllowLists = IsUnlocked(FighterConfig, "lists");

			auto Program = std::make_shared<FightScript::FProgram>();
			FightScript::FCompileError CompileError;
			if (!FightScript::Compile(FighterConfig.Script, F.Builtins, Options, *Program, CompileError))
			{
				OutErrorFighter = i;
				OutErrorLine = CompileError.Line;
				OutErrorMessage = CompileError.Message;
				return false;
			}
			F.VM.Load(Program, &F.Builtins);
		}
		return true;
	}

	static int ActionCost(const FRules& Rules, EAction Action)
	{
		switch (Action)
		{
		case EAction::Attack: return Rules.AttackCost;
		case EAction::Dash: return Rules.DashCost;
		case EAction::Heavy: return Rules.HeavyCost;
		case EAction::Heal: return Rules.HealCost;
		case EAction::Fireball: return Rules.FireballCost;
		default: return 0;
		}
	}

	void FMatch::DecideAction(int Index, std::array<FValue, 2>& Results, std::array<bool, 2>& HasResult)
	{
		FFighter& F = Fighters[static_cast<size_t>(Index)];
		HasResult[static_cast<size_t>(Index)] = false;
		Results[static_cast<size_t>(Index)] = FValue::MakeNone();

		if (F.BusyTicks > 0)
		{
			--F.BusyTicks;
			if (F.BusyTicks == 0)
			{
				F.Requested = F.PendingRelease;
				F.PendingRelease = EAction::Idle;
				HasResult[static_cast<size_t>(Index)] = true;
			}
			else
			{
				F.Requested = F.PendingRelease == EAction::Heavy ? EAction::Charge : EAction::Channel;
			}
			return;
		}

		if (F.bHalted)
		{
			F.Requested = EAction::Idle;
			return;
		}

		const EVMStatus Status = F.VM.Run(F.OpsPerTick);
		switch (Status)
		{
		case EVMStatus::Yielded:
			HasResult[static_cast<size_t>(Index)] = true;
			break;
		case EVMStatus::OutOfOps:
			F.Requested = EAction::Think;
			return;
		case EVMStatus::Finished:
			F.Requested = EAction::Idle;
			F.bHalted = true;
			AddEvent(EEventType::ScriptEnded, Index, "script finished");
			return;
		case EVMStatus::Error:
		default:
			F.Requested = EAction::Idle;
			F.bHalted = true;
			AddEvent(EEventType::Error, Index, F.VM.GetError(), F.VM.GetErrorLine());
			return;
		}

		const int Cost = ActionCost(Config.Rules, F.Requested);
		if (F.Energy < Cost)
		{
			AddEvent(EEventType::Tired, Index, ActionName(F.Requested), Cost);
			F.Requested = EAction::Tired;
			Results[static_cast<size_t>(Index)] = FValue::MakeBool(false);
			return;
		}
		F.Energy -= Cost;

		if (F.Requested == EAction::Heavy || F.Requested == EAction::Heal)
		{
			// Two-tick actions: wind up now, land next tick.
			F.PendingRelease = F.Requested;
			F.BusyTicks = 1;
			F.Requested = F.Requested == EAction::Heavy ? EAction::Charge : EAction::Channel;
			HasResult[static_cast<size_t>(Index)] = false;
			AddEvent(EEventType::Charging, Index, ActionName(F.Requested));
		}
	}

	void FMatch::Step()
	{
		if (bOver)
		{
			return;
		}
		++Tick;

		const FRules& Rules = Config.Rules;
		std::array<FValue, 2> Results;
		std::array<bool, 2> HasResult = { false, false };
		DecideAction(0, Results, HasResult);
		DecideAction(1, Results, HasResult);

		std::array<EAction, 2> Acts = { Fighters[0].Requested, Fighters[1].Requested };

		// 1) Blocking
		for (size_t i = 0; i < 2; ++i)
		{
			Fighters[i].bBlocking = Acts[i] == EAction::Block;
		}

		// 2) Movement (simultaneous, conflicting moves are cancelled)
		std::array<bool, 2> bWants = { false, false };
		std::array<int, 4> Targets = { Fighters[0].X, Fighters[0].Y, Fighters[1].X, Fighters[1].Y };
		for (size_t i = 0; i < 2; ++i)
		{
			if (Acts[i] != EAction::Move && Acts[i] != EAction::Dash)
			{
				continue;
			}
			FFighter& F = Fighters[i];
			const FFighter& Enemy = Fighters[1 - i];
			F.Facing = F.RequestedDir;
			int Dx = 0;
			int Dy = 0;
			DirDelta(F.RequestedDir, Dx, Dy);
			int Tx = F.X;
			int Ty = F.Y;
			const int Steps = Acts[i] == EAction::Dash ? 2 : 1;
			for (int s = 0; s < Steps; ++s)
			{
				const int Nx = Tx + Dx;
				const int Ny = Ty + Dy;
				if (!InBounds(Nx, Ny) || (Nx == Enemy.X && Ny == Enemy.Y))
				{
					break;
				}
				Tx = Nx;
				Ty = Ny;
			}
			Targets[i * 2] = Tx;
			Targets[i * 2 + 1] = Ty;
			bWants[i] = Tx != F.X || Ty != F.Y;
		}
		if (bWants[0] && bWants[1])
		{
			const bool bSameCell = Targets[0] == Targets[2] && Targets[1] == Targets[3];
			const bool bSwap = Targets[0] == Fighters[1].X && Targets[1] == Fighters[1].Y &&
				Targets[2] == Fighters[0].X && Targets[3] == Fighters[0].Y;
			if (bSwap)
			{
				bWants = { false, false };
			}
			else if (bSameCell)
			{
				// Both step into the same cell: a coin flip decides who gets it,
				// otherwise two identical scripts would mirror each other forever.
				uint32_t X = RandomState;
				X ^= X << 13;
				X ^= X >> 17;
				X ^= X << 5;
				RandomState = X;
				bWants[(X >> 8) & 1u] = false;
			}
		}
		for (int Pass = 0; Pass < 2; ++Pass)
		{
			for (size_t i = 0; i < 2; ++i)
			{
				const size_t o = 1 - i;
				const int OtherX = bWants[o] ? Targets[o * 2] : Fighters[o].X;
				const int OtherY = bWants[o] ? Targets[o * 2 + 1] : Fighters[o].Y;
				if (bWants[i] && Targets[i * 2] == OtherX && Targets[i * 2 + 1] == OtherY)
				{
					bWants[i] = false;
				}
			}
		}
		for (size_t i = 0; i < 2; ++i)
		{
			if (Acts[i] == EAction::Move || Acts[i] == EAction::Dash)
			{
				if (bWants[i])
				{
					Fighters[i].X = Targets[i * 2];
					Fighters[i].Y = Targets[i * 2 + 1];
					AddEvent(EEventType::Moved, static_cast<int>(i), ActionName(Acts[i]), 0, -1, Fighters[i].X, Fighters[i].Y);
				}
				Results[i] = FValue::MakeBool(bWants[i]);
			}
		}

		// 3) Attacks (resolved after movement, so stepping away dodges)
		std::array<int, 2> Damage = { 0, 0 };
		const int Dist = Distance();
		for (size_t i = 0; i < 2; ++i)
		{
			FFighter& F = Fighters[i];
			FFighter& Enemy = Fighters[1 - i];
			const int Me = static_cast<int>(i);
			const int Them = static_cast<int>(1 - i);

			if (Acts[i] == EAction::Attack || Acts[i] == EAction::Heavy)
			{
				F.Facing = DirTowards(F.X, F.Y, Enemy.X, Enemy.Y);
				const bool bHeavy = Acts[i] == EAction::Heavy;
				if (Dist != 1)
				{
					AddEvent(EEventType::Missed, Me, ActionName(Acts[i]));
					Results[i] = FValue::MakeBool(false);
					continue;
				}
				if (Enemy.bBlocking)
				{
					const int Through = bHeavy ? Rules.HeavyBlockedDamage : 0;
					Damage[1 - i] += Through;
					if (!bHeavy)
					{
						F.Energy = F.Energy > Rules.ParryEnergyLoss ? F.Energy - Rules.ParryEnergyLoss : 0;
					}
					AddEvent(EEventType::Blocked, Me, ActionName(Acts[i]), Through, Them);
					Results[i] = FValue::MakeBool(bHeavy);
					continue;
				}
				const int Amount = bHeavy ? Rules.HeavyDamage : Rules.AttackDamage;
				Damage[1 - i] += Amount;
				AddEvent(EEventType::Hit, Me, ActionName(Acts[i]), Amount, Them);
				Results[i] = FValue::MakeBool(true);
			}
			else if (Acts[i] == EAction::Fireball)
			{
				F.Facing = F.RequestedDir;
				int Dx = 0;
				int Dy = 0;
				DirDelta(F.RequestedDir, Dx, Dy);
				int Cx = F.X;
				int Cy = F.Y;
				bool bHit = false;
				for (int s = 0; s < Rules.FireballRange; ++s)
				{
					const int Nx = Cx + Dx;
					const int Ny = Cy + Dy;
					if (!InBounds(Nx, Ny))
					{
						break;
					}
					Cx = Nx;
					Cy = Ny;
					if (Cx == Enemy.X && Cy == Enemy.Y)
					{
						bHit = true;
						break;
					}
				}
				int Amount = 0;
				if (bHit)
				{
					Amount = Enemy.bBlocking ? Rules.FireballBlockedDamage : Rules.FireballDamage;
					Damage[1 - i] += Amount;
				}
				AddEvent(EEventType::Fireball, Me, Enemy.bBlocking && bHit ? "blocked" : "", Amount, bHit ? Them : -1, Cx, Cy);
				Results[i] = FValue::MakeBool(bHit);
			}
		}
		for (size_t i = 0; i < 2; ++i)
		{
			Fighters[i].Hp -= Damage[i];
		}

		// 4) Heals, energy, bookkeeping
		for (size_t i = 0; i < 2; ++i)
		{
			FFighter& F = Fighters[i];
			if (Acts[i] == EAction::Heal && F.Hp > 0)
			{
				const int Before = F.Hp;
				F.Hp = F.Hp + Rules.HealAmount > F.MaxHp ? F.MaxHp : F.Hp + Rules.HealAmount;
				AddEvent(EEventType::Healed, static_cast<int>(i), "", F.Hp - Before);
				Results[i] = FValue::MakeBool(true);
			}
			if (Acts[i] != EAction::Block)
			{
				F.Energy += Rules.EnergyRegen;
			}
			if (Acts[i] == EAction::Wait)
			{
				F.Energy += Rules.WaitBonusRegen;
			}
			if (F.Energy > Rules.MaxEnergy)
			{
				F.Energy = Rules.MaxEnergy;
			}
			F.LastAction = Acts[i];
			if (HasResult[i])
			{
				F.VM.SetYieldResult(Results[i]);
			}
		}

		// 5) End conditions
		const bool bDead0 = Fighters[0].Hp <= 0;
		const bool bDead1 = Fighters[1].Hp <= 0;
		if (bDead0 || bDead1)
		{
			Finish(bDead0 && bDead1 ? -1 : (bDead0 ? 1 : 0), "knockout");
			return;
		}
		if (Fighters[0].bHalted || Fighters[1].bHalted)
		{
			if (Fighters[0].bHalted && Fighters[1].bHalted)
			{
				Finish(-1, "both scripts stopped");
			}
			else
			{
				Finish(Fighters[0].bHalted ? 1 : 0, "script stopped");
			}
			return;
		}
		if (Tick >= Config.MaxTicks)
		{
			const double Ratio0 = static_cast<double>(Fighters[0].Hp) / Fighters[0].MaxHp;
			const double Ratio1 = static_cast<double>(Fighters[1].Hp) / Fighters[1].MaxHp;
			Finish(Ratio0 == Ratio1 ? -1 : (Ratio0 > Ratio1 ? 0 : 1), "time limit");
		}
	}

	void FMatch::Finish(int InWinner, const std::string& Reason)
	{
		bOver = true;
		Winner = InWinner;
		AddEvent(EEventType::MatchOver, InWinner < 0 ? 0 : InWinner, Reason, 0, InWinner);
	}
}
