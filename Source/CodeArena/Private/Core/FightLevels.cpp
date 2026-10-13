#include "FightLevels.h"

namespace FightLevels
{
	static FLevelDef MakeLevel(const char* Name, int Hp, int Reward, int RepeatReward, const char* Script)
	{
		FLevelDef Level;
		Level.EnemyName = Name;
		Level.EnemyHp = Hp;
		Level.Reward = Reward;
		Level.RepeatReward = RepeatReward;
		Level.EnemyScript = Script;
		return Level;
	}

	const std::vector<FLevelDef>& GetLevels()
	{
		static const std::vector<FLevelDef> Levels = {
			MakeLevel("Dummy", 40, 40, 5,
R"(# Training dummy: it just stands there.
while True:
    wait()
)"),

			MakeLevel("Brawler", 70, 60, 10,
R"(# Walks straight at you and swings when it can.
while True:
    if distance() > 1:
        move(dir_to_enemy())
    elif my_energy() >= 15:
        attack()
    else:
        wait()
)"),

			MakeLevel("Turtle", 130, 90, 15,
R"(# Fixed rhythm when adjacent: block, block, attack, rest.
n = 0
while True:
    if distance() > 1:
        move(dir_to_enemy())
    else:
        n += 1
        step = n % 4
        if step == 1 or step == 2:
            block()
        elif step == 3:
            attack()
        else:
            wait()
)"),

			MakeLevel("Runner", 90, 120, 20,
R"(# Hit and run: strikes once, then dashes away to recover.
def flee():
    away = opposite(dir_to_enemy())
    if can_move(away):
        dash(away)
    elif can_move(North):
        move(North)
    else:
        move(South)

while True:
    d = distance()
    if d == 1:
        if my_energy() >= 40:
            attack()
        flee()
    elif my_energy() >= 60:
        move(dir_to_enemy())
    else:
        wait()
)"),

			MakeLevel("Witch", 90, 160, 25,
R"(# Keeps her distance and throws fireballs along rows and columns.
def align_move():
    dx = enemy_x() - my_x()
    dy = enemy_y() - my_y()
    if dx == 0 or dy == 0:
        return dir_to_enemy()
    if abs(dx) <= abs(dy):
        if dx > 0:
            return East
        return West
    if dy > 0:
        return North
    return South

while True:
    d = distance()
    lined_up = my_x() == enemy_x() or my_y() == enemy_y()
    if d == 1:
        away = opposite(dir_to_enemy())
        if my_energy() >= 25 and can_move(away):
            dash(away)
        else:
            block()
    elif lined_up and d <= 4:
        if my_energy() >= 30:
            fireball(dir_to_enemy())
        else:
            wait()
    else:
        move(align_move())
)"),

			MakeLevel("Champion", 150, 250, 40,
R"(# Reads your moves, punishes tired opponents and heals when safe.
while True:
    d = distance()
    seen = enemy_action()
    lined_up = my_x() == enemy_x() or my_y() == enemy_y()
    if my_hp() < 60 and my_energy() >= 50 and d > 2:
        heal()
    elif d > 1:
        if lined_up and d <= 4 and my_energy() >= 70:
            fireball(dir_to_enemy())
        else:
            move(dir_to_enemy())
    elif seen == "charge":
        away = opposite(dir_to_enemy())
        if my_energy() >= 25 and can_move(away):
            dash(away)
        else:
            block()
    elif enemy_energy() < 15 and my_energy() >= 40:
        heavy_attack()
    elif my_energy() >= 15:
        attack()
    else:
        block()
)"),
		};
		return Levels;
	}

	FightSim::FMatchConfig MakeMatchConfig(const FLevelDef& Level, const FightSim::FFighterConfig& Player, uint32_t Seed)
	{
		FightSim::FMatchConfig Config;
		Config.Width = Level.Width;
		Config.Height = Level.Height;
		Config.MaxTicks = Level.MaxTicks;
		Config.Seed = Seed;
		Config.Fighters[0] = Player;

		FightSim::FFighterConfig& Enemy = Config.Fighters[1];
		Enemy.Name = Level.EnemyName;
		Enemy.Script = Level.EnemyScript;
		Enemy.MaxHp = Level.EnemyHp;
		Enemy.OpsPerTick = Level.EnemyOps;
		Enemy.bAllUnlocked = true;

		const int MidY = Level.Height / 2;
		Config.StartPositions = { 1, MidY, Level.Width - 2, MidY };
		return Config;
	}
}
