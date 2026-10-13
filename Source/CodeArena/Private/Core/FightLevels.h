// Campaign levels. Every enemy is just another FightScript program, so the
// enemies double as examples of what the player can write.

#pragma once

#include "FightSim.h"

#include <string>
#include <vector>

namespace FightLevels
{
	struct FLevelDef
	{
		std::string EnemyName;
		std::string EnemyScript;
		int EnemyHp = 100;
		int EnemyOps = 1000;
		int Width = 7;
		int Height = 7;
		int MaxTicks = 300;
		int Reward = 50;       // gold for the first win
		int RepeatReward = 10; // gold for every later win
	};

	const std::vector<FLevelDef>& GetLevels();

	// Builds a ready-to-run match config for a level.
	FightSim::FMatchConfig MakeMatchConfig(const FLevelDef& Level, const FightSim::FFighterConfig& Player, uint32_t Seed);
}
