// Turn-based arena simulation. Two fighters, each driven by a FightScript program.
// Every tick both scripts run until they choose an action (or run out of
// operations), then all actions are resolved at the same time.
// Plain C++ so it can be tested outside Unreal.

#pragma once

#include "FightScript.h"

#include <array>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace FightSim
{
	enum class EDir : uint8_t { North, East, South, West };

	enum class EAction : uint8_t
	{
		Idle,     // script finished / crashed
		Think,    // script ran out of operations this tick
		Wait,
		Move,
		Attack,
		Block,
		Dash,
		Charge,   // first tick of heavy_attack()
		Heavy,    // heavy attack lands
		Channel,  // first tick of heal()
		Heal,     // heal lands
		Fireball,
		Tired     // not enough energy for the requested action
	};

	const char* ActionName(EAction Action);
	const char* DirName(EDir Dir);

	// Gameplay numbers, kept together so they are easy to tune.
	struct FRules
	{
		int AttackDamage = 10;
		int HeavyDamage = 28;
		int HeavyBlockedDamage = 10;
		int FireballDamage = 12;
		int FireballBlockedDamage = 4;
		int FireballRange = 4;
		int HealAmount = 25;

		int AttackCost = 15;
		int DashCost = 25;
		int HeavyCost = 40;
		int HealCost = 50;
		int FireballCost = 30;
		int ParryEnergyLoss = 15; // attacker loses this when hitting a block

		int MaxEnergy = 100;
		int EnergyRegen = 5;
		int WaitBonusRegen = 10;
	};

	struct FFighterConfig
	{
		std::string Name = "Fighter";
		std::string Script;
		int MaxHp = 100;
		int OpsPerTick = 100;
		bool bAllUnlocked = false;
		std::set<std::string> Unlocks; // see UnlockIds below
	};

	// Unlock ids used by FFighterConfig::Unlocks:
	//   block, dash, scanner, radar, heavy, heal, fireball   -> builtins
	//   for, def, lists                                      -> language features
	bool IsUnlocked(const FFighterConfig& Config, const std::string& Id);

	struct FMatchConfig
	{
		int Width = 7;
		int Height = 7;
		int MaxTicks = 300;
		uint32_t Seed = 12345;
		FRules Rules;
		std::array<FFighterConfig, 2> Fighters;
		std::array<int, 4> StartPositions = { 1, 3, 5, 3 }; // x0, y0, x1, y1
	};

	enum class EEventType : uint8_t
	{
		Print,      // Text
		Error,      // Text, Amount = line
		Moved,      // X, Y
		Hit,        // Target, Amount
		Blocked,    // Target, Amount (damage that went through)
		Missed,
		Healed,     // Amount
		Fireball,   // X, Y = impact cell, Target = -1 when it missed
		Charging,
		Tired,
		ScriptEnded,
		MatchOver   // Target = winner (-1 draw)
	};

	struct FEvent
	{
		EEventType Type = EEventType::Print;
		int Fighter = 0;
		int Target = -1;
		int Amount = 0;
		int X = 0;
		int Y = 0;
		std::string Text;
	};

	struct FFighter
	{
		std::string Name;
		int X = 0;
		int Y = 0;
		int Hp = 100;
		int MaxHp = 100;
		int Energy = 100;
		int OpsPerTick = 100;
		EDir Facing = EDir::East;
		EAction LastAction = EAction::Idle;
		bool bBlocking = false;
		bool bHalted = false;

		// Action requested by the script during this tick.
		EAction Requested = EAction::Idle;
		EDir RequestedDir = EDir::North;
		// Multi-tick actions (heavy_attack, heal) keep the fighter busy.
		EAction PendingRelease = EAction::Idle;
		int BusyTicks = 0;

		FightScript::FBuiltinTable Builtins;
		FightScript::FVM VM;
	};

	class FMatch
	{
	public:
		FMatch() = default;
		FMatch(const FMatch&) = delete;
		FMatch& operator=(const FMatch&) = delete;

		// Compiles both scripts. On failure fills the error info and returns false.
		bool Init(const FMatchConfig& InConfig, int& OutErrorFighter, int& OutErrorLine, std::string& OutErrorMessage);

		// Advances one tick.
		void Step();

		bool IsOver() const { return bOver; }
		int GetWinner() const { return Winner; } // 0, 1 or -1 (draw)
		int GetTick() const { return Tick; }
		const FMatchConfig& GetConfig() const { return Config; }
		const FFighter& GetFighter(int Index) const { return Fighters[static_cast<size_t>(Index)]; }
		int GetCurrentLine(int Index) const { return Fighters[static_cast<size_t>(Index)].VM.GetCurrentLine(); }

		// Returns and clears the events produced since the last call.
		std::vector<FEvent> ConsumeEvents();

	private:
		void BindBuiltins(int Index);
		void DecideAction(int Index, std::array<FightScript::FValue, 2>& Results, std::array<bool, 2>& HasResult);
		bool InBounds(int X, int Y) const;
		static void DirDelta(EDir Dir, int& OutDx, int& OutDy);
		EDir DirTowards(int FromX, int FromY, int ToX, int ToY) const;
		int Distance() const;
		void Finish(int InWinner, const std::string& Reason);
		void AddEvent(EEventType Type, int Fighter, const std::string& Text = std::string(), int Amount = 0, int Target = -1, int X = 0, int Y = 0);

		FMatchConfig Config;
		std::array<FFighter, 2> Fighters;
		std::vector<FEvent> Events;
		uint32_t RandomState = 1;
		int Tick = 0;
		bool bOver = false;
		int Winner = -1;
	};
}
