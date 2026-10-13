// Standalone tests for the engine-independent core (language + simulation).
// Build & run:  ./CodeArena/Tests/run_tests.sh

#include "FightLevels.h"
#include "FightScript.h"
#include "FightSim.h"

#include <cstdio>
#include <string>
#include <vector>

static int Failures = 0;

static void Expect(bool bCondition, const std::string& What)
{
	if (!bCondition)
	{
		++Failures;
		std::printf("  FAIL: %s\n", What.c_str());
	}
}

// Runs a script to completion with the standard library only and returns the printed output.
static std::string RunScript(const std::string& Source, std::string* OutError = nullptr, FightScript::FCompileOptions Options = {})
{
	std::string Output;
	uint32_t Seed = 42;
	FightScript::FBuiltinTable Table;
	FightScript::AddStandardLibrary(Table, [&Output](const std::string& Line) { Output += Line + "\n"; }, &Seed);

	auto Program = std::make_shared<FightScript::FProgram>();
	FightScript::FCompileError CompileError;
	if (!FightScript::Compile(Source, Table, Options, *Program, CompileError))
	{
		if (OutError)
		{
			*OutError = "line " + std::to_string(CompileError.Line) + ": " + CompileError.Message;
		}
		return Output;
	}
	FightScript::FVM VM;
	VM.Load(Program, &Table);
	for (int i = 0; i < 10000 && !VM.IsHalted(); ++i)
	{
		VM.Run(1000);
	}
	if (VM.GetStatus() == FightScript::EVMStatus::Error && OutError)
	{
		*OutError = "line " + std::to_string(VM.GetErrorLine()) + ": " + VM.GetError();
	}
	return Output;
}

static void ExpectOutput(const char* Name, const std::string& Source, const std::string& Expected)
{
	std::string Error;
	const std::string Output = RunScript(Source, &Error);
	Expect(Error.empty(), std::string(Name) + " raised an error: " + Error);
	Expect(Output == Expected, std::string(Name) + " output was:\n" + Output + "expected:\n" + Expected);
}

static void ExpectError(const char* Name, const std::string& Source, const std::string& Fragment, FightScript::FCompileOptions Options = {})
{
	std::string Error;
	RunScript(Source, &Error, Options);
	Expect(Error.find(Fragment) != std::string::npos, std::string(Name) + " expected error containing '" + Fragment + "' got '" + Error + "'");
}

static void LanguageTests()
{
	std::printf("Language tests\n");

	ExpectOutput("arithmetic", "print(1 + 2 * 3, 7 // 2, 7 % 3, -7 % 3, 10 / 4, -(2 + 3))\n", "7 3 1 2 2.5 -5\n");
	ExpectOutput("strings", "s = 'ab' + \"cd\"\nprint(s, len(s), str(5) + '!', s[1])\n", "abcd 4 5! b\n");
	ExpectOutput("bools", "print(1 < 2 and 3 > 4, not False, 1 == 1.0, None == None, 2 in [1, 2])\n", "False True True True True\n");
	ExpectOutput("if/elif/else",
		"for x in range(4):\n"
		"    if x == 0:\n"
		"        print('zero')\n"
		"    elif x == 1:\n"
		"        print('one')\n"
		"    else:\n"
		"        print('many', x)\n",
		"zero\none\nmany 2\nmany 3\n");
	ExpectOutput("while/break/continue",
		"i = 0\n"
		"total = 0\n"
		"while True:\n"
		"    i += 1\n"
		"    if i % 2 == 0:\n"
		"        continue\n"
		"    if i > 9:\n"
		"        break\n"
		"    total += i\n"
		"print(total)\n",
		"25\n");
	ExpectOutput("for break inside function",
		"def first_over(items, limit):\n"
		"    for v in items:\n"
		"        if v > limit:\n"
		"            return v\n"
		"    return -1\n"
		"print(first_over([1, 5, 9], 4), first_over([1], 4))\n"
		"for i in range(10):\n"
		"    if i == 3:\n"
		"        break\n"
		"print(i)\n",
		"5 -1\n3\n");
	ExpectOutput("recursion",
		"def fib(n):\n"
		"    if n < 2:\n"
		"        return n\n"
		"    return fib(n - 1) + fib(n - 2)\n"
		"print(fib(15))\n",
		"610\n");
	ExpectOutput("globals",
		"count = 0\n"
		"def bump():\n"
		"    global count\n"
		"    count += 1\n"
		"def peek():\n"
		"    return count * 10\n"
		"bump()\n"
		"bump()\n"
		"print(count, peek())\n",
		"2 20\n");
	ExpectOutput("locals shadow globals",
		"x = 1\n"
		"def f():\n"
		"    x = 5\n"
		"    return x\n"
		"print(f(), x)\n",
		"5 1\n");
	ExpectOutput("lists",
		"a = [3, 1, 2]\n"
		"a.append(7)\n"
		"a[0] = 10\n"
		"a[1] += 5\n"
		"print(a, len(a), a[-1], a.pop(), a, min(a), max(4, 9, 2), a.index(2))\n",
		"[10, 6, 2] 4 7 7 [10, 6, 2] 2 9 2\n");
	ExpectOutput("one-line blocks", "if True: print('yes')\nwhile False: pass\n", "yes\n");
	ExpectOutput("comments & blank lines", "# hello\n\nx = 2  # two\n\n    # indented comment\nprint(x)\n", "2\n");
	ExpectOutput("multiline call", "print(1,\n      2)\n", "1 2\n");

	ExpectError("name error", "print(y)\n", "NameError");
	ExpectError("zero division", "x = 1 / 0\n", "ZeroDivisionError");
	ExpectError("bad indent", "if True:\nprint(1)\n", "IndentationError");
	ExpectError("unexpected indent", "x = 1\n    y = 2\n", "unexpected indent");
	ExpectError("type error", "x = 'a' + 1\n", "TypeError");
	ExpectError("arity", "def f(a):\n    return a\nf()\n", "takes 1");
	ExpectError("recursion limit", "def f():\n    return f()\nf()\n", "RecursionError");
	ExpectError("index", "a = [1]\nprint(a[3])\n", "IndexError");
	ExpectError("else without if", "else:\n    pass\n", "without a matching");
	ExpectError("accent", "pontuação = 1\n", "ASCII");

	FightScript::FCompileOptions Locked;
	Locked.bAllowFor = false;
	Locked.bAllowDef = false;
	Locked.bAllowLists = false;
	ExpectError("locked for", "for i in range(3):\n    pass\n", "Locked", Locked);
	ExpectError("locked def", "def f():\n    pass\n", "Locked", Locked);
	ExpectError("locked lists", "a = [1]\n", "Locked", Locked);

	// Op budget: an infinite loop must not hang the VM.
	{
		uint32_t Seed = 1;
		FightScript::FBuiltinTable Table;
		FightScript::AddStandardLibrary(Table, nullptr, &Seed);
		auto Program = std::make_shared<FightScript::FProgram>();
		FightScript::FCompileError Err;
		Expect(FightScript::Compile("while True:\n    pass\n", Table, {}, *Program, Err), "infinite loop compiles");
		FightScript::FVM VM;
		VM.Load(Program, &Table);
		Expect(VM.Run(50) == FightScript::EVMStatus::OutOfOps, "infinite loop yields OutOfOps");
		Expect(VM.GetTotalOps() == 50, "op budget respected");
	}
}

static const char* StarterScript = R"(
while True:
    if distance() > 1:
        if enemy_x() > my_x():
            move(East)
        elif enemy_x() < my_x():
            move(West)
        elif enemy_y() > my_y():
            move(North)
        else:
            move(South)
    elif my_energy() >= 15:
        attack()
    else:
        wait()
)";

static const char* TurtleSolver = R"(
# Hit the turtle only on the tick after it attacks (it rests then).
while True:
    if distance() > 1:
        move(dir_to_enemy())
    elif enemy_action() == "attack" and my_energy() >= 15:
        attack()
    elif enemy_action() == "block":
        block()
    else:
        wait()
)";

static const char* AdvancedScript = R"(
def lined_up():
    return my_x() == enemy_x() or my_y() == enemy_y()

def chase():
    if lined_up() and distance() <= 4 and distance() > 1 and my_energy() >= 45:
        fireball(dir_to_enemy())
    else:
        move(dir_to_enemy())

while True:
    d = distance()
    seen = enemy_action()
    if my_hp() < 45 and my_energy() >= 50 and d > 2:
        heal()
    elif d > 1:
        chase()
    elif seen == "charge":
        block()
    elif seen == "block" or seen == "tired" or seen == "wait":
        if my_energy() >= 40:
            heavy_attack()
        else:
            wait()
    elif my_energy() >= 15:
        attack()
    else:
        block()
)";

static int RunLevel(size_t LevelIndex, const char* Script, bool bAllUnlocked, int OpsPerTick, bool bVerbose)
{
	const FightLevels::FLevelDef& Level = FightLevels::GetLevels()[LevelIndex];
	FightSim::FFighterConfig Player;
	Player.Name = "Player";
	Player.Script = Script;
	Player.OpsPerTick = OpsPerTick;
	Player.bAllUnlocked = bAllUnlocked;

	FightSim::FMatch Match;
	int ErrFighter = -1;
	int ErrLine = 0;
	std::string ErrMessage;
	if (!Match.Init(FightLevels::MakeMatchConfig(Level, Player, 777), ErrFighter, ErrLine, ErrMessage))
	{
		std::printf("  compile error (fighter %d) line %d: %s\n", ErrFighter, ErrLine, ErrMessage.c_str());
		return -2;
	}
	while (!Match.IsOver())
	{
		Match.Step();
		for (const FightSim::FEvent& Event : Match.ConsumeEvents())
		{
			if (Event.Type == FightSim::EEventType::Error)
			{
				std::printf("  runtime error (fighter %d) line %d: %s\n", Event.Fighter, Event.Amount, Event.Text.c_str());
			}
			else if (bVerbose && Event.Type == FightSim::EEventType::Print)
			{
				std::printf("  print: %s\n", Event.Text.c_str());
			}
		}
	}
	std::printf("  %-9s vs %-8s -> winner %2d at tick %3d (hp %3d vs %3d)\n", bAllUnlocked ? "advanced" : "starter",
		Level.EnemyName.c_str(), Match.GetWinner(), Match.GetTick(), Match.GetFighter(0).Hp, Match.GetFighter(1).Hp);
	return Match.GetWinner();
}

static void SimulationTests()
{
	std::printf("Simulation tests\n");
	const auto& Levels = FightLevels::GetLevels();

	// Every enemy script must compile and run without errors.
	for (size_t i = 0; i < Levels.size(); ++i)
	{
		const int Winner = RunLevel(i, AdvancedScript, true, 1000, false);
		Expect(Winner != -2, "level " + std::to_string(i) + " compiles");
	}

	Expect(RunLevel(0, StarterScript, false, 100, false) == 0, "starter script beats the dummy");
	Expect(RunLevel(1, StarterScript, false, 100, false) == 0, "starter script beats the brawler");
	Expect(RunLevel(2, StarterScript, false, 100, false) != 0, "starter script should NOT beat the turtle");

	FightSim::FFighterConfig Player;
	Player.Script = TurtleSolver;
	Player.Unlocks = { "scanner", "radar", "block" };
	FightSim::FMatch Match;
	int ErrFighter = 0;
	int ErrLine = 0;
	std::string ErrMessage;
	Expect(Match.Init(FightLevels::MakeMatchConfig(Levels[2], Player, 1), ErrFighter, ErrLine, ErrMessage), "turtle solver compiles");
	while (!Match.IsOver())
	{
		Match.Step();
	}
	std::printf("  solver    vs Turtle   -> winner %2d at tick %3d (hp %3d vs %3d)\n", Match.GetWinner(), Match.GetTick(),
		Match.GetFighter(0).Hp, Match.GetFighter(1).Hp);
	Expect(Match.GetWinner() == 0, "scanner-based script beats the turtle");

	// A locked builtin is a runtime error that makes the player forfeit.
	FightSim::FFighterConfig LockedPlayer;
	LockedPlayer.Script = "while True:\n    block()\n";
	FightSim::FMatch LockedMatch;
	Expect(LockedMatch.Init(FightLevels::MakeMatchConfig(Levels[0], LockedPlayer, 1), ErrFighter, ErrLine, ErrMessage), "locked script compiles");
	LockedMatch.Step();
	bool bSawLocked = false;
	for (const FightSim::FEvent& Event : LockedMatch.ConsumeEvents())
	{
		bSawLocked |= Event.Type == FightSim::EEventType::Error && Event.Text.find("Locked") != std::string::npos;
	}
	Expect(bSawLocked, "calling a locked builtin reports an error");
	Expect(LockedMatch.IsOver() && LockedMatch.GetWinner() == 1, "a crashed script forfeits");

	// Action results: move() returns False when walking into a wall.
	FightSim::FFighterConfig WallPlayer;
	WallPlayer.Script = "a = move(West)\nb = move(West)\nprint(a, b)\nwhile True:\n    wait()\n";
	FightSim::FMatch WallMatch;
	Expect(WallMatch.Init(FightLevels::MakeMatchConfig(Levels[0], WallPlayer, 1), ErrFighter, ErrLine, ErrMessage), "wall script compiles");
	std::string Printed;
	for (int i = 0; i < 4; ++i)
	{
		WallMatch.Step();
		for (const FightSim::FEvent& Event : WallMatch.ConsumeEvents())
		{
			if (Event.Type == FightSim::EEventType::Print)
			{
				Printed += Event.Text;
			}
		}
	}
	Expect(Printed == "True False", "move() result values, got '" + Printed + "'");
}

int main()
{
	LanguageTests();
	SimulationTests();
	if (Failures == 0)
	{
		std::printf("\nALL TESTS PASSED\n");
		return 0;
	}
	std::printf("\n%d FAILURE(S)\n", Failures);
	return 1;
}
