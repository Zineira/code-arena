#include "CodeArenaContent.h"

#define LOCTEXT_NAMESPACE "CodeArena"

namespace CodeArenaContent
{
	FLevelText GetLevelText(int32 LevelIndex)
	{
		switch (LevelIndex)
		{
		case 0:
			return {
				LOCTEXT("L0T", "Boneco de Treino"),
				LOCTEXT("L0D", "Um boneco que não faz nada. Escreve um programa que se aproxime e o ataque.\nDica: while True: repete o bloco para sempre.")
			};
		case 1:
			return {
				LOCTEXT("L1T", "Arruaceiro"),
				LOCTEXT("L1D", "Vem direito a ti e ataca sempre que pode. Gere a energia: attack() custa 15 e regeneras 5 por turno (15 com wait()). Atacar sem energia falha!")
			};
		case 2:
			return {
				LOCTEXT("L2T", "Tartaruga"),
				LOCTEXT("L2D", "Defende-se num ritmo fixo: bloqueia, bloqueia, ataca, descansa. Bater num escudo não faz dano e cansa-te. Compra o Scanner e usa enemy_action() para atacar no momento certo.")
			};
		case 3:
			return {
				LOCTEXT("L3T", "Fugitivo"),
				LOCTEXT("L3D", "Bate e foge com dash(). Encurrala-o contra as paredes ou castiga-o quando ficar sem energia (enemy_energy()).")
			};
		case 4:
			return {
				LOCTEXT("L4T", "Feiticeira"),
				LOCTEXT("L4D", "Mantém a distância e lança bolas de fogo ao longo das linhas e colunas. Evita ficar alinhado com ela enquanto te aproximas!")
			};
		case 5:
			return {
				LOCTEXT("L5T", "Campeão"),
				LOCTEXT("L5D", "Lê os teus movimentos, castiga quem está cansado, foge de golpes pesados e cura-se quando está seguro. Usa tudo o que aprendeste.")
			};
		default:
			return { FText::FromString(TEXT("?")), FText::GetEmpty() };
		}
	}

	const TArray<FUnlockDef>& GetUnlocks()
	{
		static TArray<FUnlockDef> Unlocks;
		if (Unlocks.Num() == 0)
		{
			const auto Add = [](const TCHAR* Id, const FText& Name, const FText& Description, int32 Cost, const TCHAR* Requires = TEXT(""))
			{
				FUnlockDef Def;
				Def.Id = Id;
				Def.Name = Name;
				Def.Description = Description;
				Def.Cost = Cost;
				Def.Requires = Requires;
				Unlocks.Add(Def);
			};

			Add(TEXT("radar"), LOCTEXT("U_radar", "Radar"),
				LOCTEXT("U_radar_d", "dir_to_enemy(): devolve a direção (North/East/South/West) que mais te aproxima do inimigo."), 15);
			Add(TEXT("for"), LOCTEXT("U_for", "Ciclos for"),
				LOCTEXT("U_for_d", "for i in range(n): repete um bloco n vezes ou percorre uma lista."), 20);
			Add(TEXT("block"), LOCTEXT("U_block", "Escudo"),
				LOCTEXT("U_block_d", "block(): bloqueia ataques neste turno. Quem bate num escudo não faz dano e perde 15 de energia."), 25);
			Add(TEXT("def"), LOCTEXT("U_def", "Funções"),
				LOCTEXT("U_def_d", "def, return e global: organiza o teu código em funções reutilizáveis."), 30);
			Add(TEXT("cpu1"), LOCTEXT("U_cpu1", "Processador I"),
				LOCTEXT("U_cpu1_d", "250 operações por turno (eram 100). Se o teu código gastar todas as operações sem agir, o lutador fica a \"pensar\" e perde o turno."), 30);
			Add(TEXT("scanner"), LOCTEXT("U_scanner", "Scanner"),
				LOCTEXT("U_scanner_d", "enemy_action() e enemy_energy(): descobre o que o inimigo fez no último turno e quanta energia tem."), 40);
			Add(TEXT("lists"), LOCTEXT("U_lists", "Listas"),
				LOCTEXT("U_lists_d", "[1, 2, 3], a[0], a.append(x), a.pop(), len(a), x in a."), 40);
			Add(TEXT("dash"), LOCTEXT("U_dash", "Investida"),
				LOCTEXT("U_dash_d", "dash(dir): avança 2 casas num só turno. Custa 25 de energia."), 50);
			Add(TEXT("armor1"), LOCTEXT("U_armor1", "Armadura I"),
				LOCTEXT("U_armor1_d", "Vida máxima 120."), 50);
			Add(TEXT("heavy"), LOCTEXT("U_heavy", "Golpe pesado"),
				LOCTEXT("U_heavy_d", "heavy_attack(): carrega durante 1 turno e acerta 28 de dano (10 mesmo contra escudo). Custa 40 de energia."), 70);
			Add(TEXT("cpu2"), LOCTEXT("U_cpu2", "Processador II"),
				LOCTEXT("U_cpu2_d", "600 operações por turno."), 70, TEXT("cpu1"));
			Add(TEXT("heal"), LOCTEXT("U_heal", "Cura"),
				LOCTEXT("U_heal_d", "heal(): concentra-te durante 1 turno e recupera 25 de vida. Custa 50 de energia."), 80);
			Add(TEXT("fireball"), LOCTEXT("U_fireball", "Bola de fogo"),
				LOCTEXT("U_fireball_d", "fireball(dir): projétil em linha reta até 4 casas, 12 de dano (4 contra escudo). Custa 30 de energia."), 100);
			Add(TEXT("armor2"), LOCTEXT("U_armor2", "Armadura II"),
				LOCTEXT("U_armor2_d", "Vida máxima 140."), 100, TEXT("armor1"));
			Add(TEXT("cpu3"), LOCTEXT("U_cpu3", "Processador III"),
				LOCTEXT("U_cpu3_d", "1500 operações por turno."), 150, TEXT("cpu2"));
		}
		return Unlocks;
	}

	FString GetStarterCode()
	{
		return TEXT(
			"# O teu lutador é controlado por este programa.\n"
			"# Carrega em Executar para começar o combate.\n"
			"# O mapa é uma grelha: x cresce para East, y cresce para North.\n"
			"\n"
			"while True:\n"
			"    if distance() > 1:\n"
			"        if enemy_x() > my_x():\n"
			"            move(East)\n"
			"        else:\n"
			"            move(West)\n"
			"    else:\n"
			"        attack()\n");
	}

	FText GetHelpText()
	{
		return LOCTEXT("Help",
			"COMO FUNCIONA\n"
			"Cada turno, o teu programa corre até chamar uma AÇÃO (move, attack, ...).\n"
			"As ações dos dois lutadores são resolvidas ao mesmo tempo: primeiro escudos,\n"
			"depois movimentos e por fim ataques (afastar-te no turno certo esquiva-te!).\n"
			"Cada instrução gasta 1 operação. Se gastares as operações do turno sem agir,\n"
			"o lutador fica a pensar e perde o turno. Se o programa acabar ou der erro, perdes.\n"
			"\n"
			"LINGUAGEM (parecida com Python)\n"
			"  x = 3        x += 1        if / elif / else        while cond:\n"
			"  and  or  not   ==  !=  <  <=  >  >=   +  -  *  /  //  %\n"
			"  for i in range(n):            (desbloquear: Ciclos for)\n"
			"  def nome(a, b): return a + b  (desbloquear: Funções)\n"
			"  [1, 2], a[0], a.append(x)     (desbloquear: Listas)\n"
			"  print(...)  str(x)  int(x)  abs(x)  min(...)  max(...)  len(x)\n"
			"  random()  randint(a, b)  break  continue  pass  # comentário\n"
			"\n"
			"AÇÕES (gastam o turno)\n"
			"  wait()               descansa: +15 de energia em vez de +5\n"
			"  move(dir)            anda 1 casa; devolve True se andou\n"
			"  attack()             10 de dano se o inimigo estiver adjacente (15 energia)\n"
			"  block()              bloqueia neste turno (não regenera energia)   [Escudo]\n"
			"  dash(dir)            anda 2 casas (25 energia)                     [Investida]\n"
			"  heavy_attack()       1 turno a carregar, depois 28 de dano (40)    [Golpe pesado]\n"
			"  heal()               1 turno a concentrar, depois +25 vida (50)    [Cura]\n"
			"  fireball(dir)        linha reta até 4 casas, 12 de dano (30)       [Bola de fogo]\n"
			"\n"
			"SENSORES (não gastam o turno)\n"
			"  my_x()  my_y()  my_hp()  my_energy()\n"
			"  enemy_x()  enemy_y()  enemy_hp()  distance()  get_tick()\n"
			"  arena_width()  arena_height()  can_move(dir)  opposite(dir)\n"
			"  dir_to_enemy()                                                    [Radar]\n"
			"  enemy_action()  enemy_energy()                                    [Scanner]\n"
			"     enemy_action() devolve: \"wait\" \"move\" \"attack\" \"block\" \"dash\"\n"
			"     \"charge\" \"heavy\" \"channel\" \"heal\" \"fireball\" \"tired\" \"think\" \"idle\"\n"
			"\n"
			"DIREÇÕES: North (y+1), East (x+1), South (y-1), West (x-1)\n"
			"ENERGIA: máximo 100. Sem energia suficiente a ação falha (\"tired\").");
	}

	int32 GetOpsPerTick(const TArray<FString>& Unlocks)
	{
		if (Unlocks.Contains(TEXT("cpu3"))) return 1500;
		if (Unlocks.Contains(TEXT("cpu2"))) return 600;
		if (Unlocks.Contains(TEXT("cpu1"))) return 250;
		return 100;
	}

	int32 GetMaxHp(const TArray<FString>& Unlocks)
	{
		if (Unlocks.Contains(TEXT("armor2"))) return 140;
		if (Unlocks.Contains(TEXT("armor1"))) return 120;
		return 100;
	}
}

#undef LOCTEXT_NAMESPACE
