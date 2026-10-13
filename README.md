# Code Arena: programa o teu lutador

Minijogo em **Unreal Engine 5** inspirado em *The Farmer Was Replaced*: em vez de cultivar uma quinta, escreves o programa que controla um **lutador** numa arena em grelha. Cada turno o teu código decide o que fazer: andar, atacar, bloquear, investir, carregar um golpe pesado, curar-se ou lançar bolas de fogo. Vences combates, ganhas ouro e compras novas funções e funcionalidades da linguagem.

A linguagem (**FightScript**) é parecida com Python: `while`, `if/elif/else`, variáveis, `for`, funções, listas, etc.

```python
# Exemplo: aproxima-te e ataca, descansando quando ficas sem energia
while True:
    if distance() > 1:
        move(dir_to_enemy())
    elif my_energy() >= 15:
        attack()
    else:
        wait()
```

---

## Jogar no browser (sem Unreal)

A pasta `web/` tem uma versão que corre em qualquer browser, sem instalar nada: o mesmo jogo, a mesma linguagem, as mesmas regras e os mesmos 6 níveis, com uma arena 2D.

- Abre `web/index.html` no browser (duplo clique serve).
- O progresso fica guardado no próprio browser.
- `web/engine.js` é a tradução direta do núcleo C++ para JavaScript; `node web/test.js` corre os mesmos testes da versão C++ e dá os mesmos resultados em todos os combates.

### Multijogador

No separador **Multijogador** cada pessoa escreve um lutador e publica-o na arena partilhada; qualquer um pode **Desafiar** os lutadores publicados (e os 6 bots da campanha) ou correr um **Torneio** de todos contra todos com ranking. Aqui está tudo desbloqueado, com 250 operações por turno e 100 de vida para todos. Como o combate é determinístico, cada browser calcula o mesmo resultado a partir dos dois códigos, por isso não é preciso servidor de jogo: só a lista de lutadores é partilhada (base de dados da página publicada no claude.ai). Aberto como ficheiro local, o multijogador funciona só contra os bots.

**Salas de combate (até 8 jogadores):** no Multijogador, combinem um nome de sala e entrem todos com o vosso lutador. A lista mostra quem está na sala em tempo real; quando estiverem prontos, qualquer um carrega em **Começar combate** e, após uma contagem de 3 segundos, todos veem a mesma batalha de todos contra todos numa arena 9×9. Nos scripts, `enemy_x()`, `distance()`, `dir_to_enemy()` e companhia referem-se ao adversário mais próximo ainda de pé, e `enemy_count()` diz quantos restam, por isso os códigos de 1 contra 1 funcionam sem mudanças.

A versão Unreal (3D) está descrita a seguir.

## Requisitos

- Unreal Engine **5.4** (funciona também com 5.3, 5.5 e 5.6: clica com o botão direito no `CodeArena.uproject` e escolhe *Switch Unreal Engine version*).
- Windows: Visual Studio 2022 com o workload *Game development with C++*.
- macOS: Xcode. Linux: o toolchain do Unreal.

Não é preciso nenhum asset nem Blueprint. Tudo (arena, lutadores, interface) é criado em C++ com as formas básicas do motor e com Slate.

## Como abrir e jogar

1. Faz duplo clique em `CodeArena.uproject` e aceita quando o editor pedir para compilar o módulo `CodeArena`.
   (Em alternativa: botão direito no `.uproject` → *Generate Visual Studio project files*, abre a `.sln` e compila `CodeArenaEditor` em *Development Editor*.)
2. O editor abre o mapa base do motor (`Template_Default`). Carrega em **Play**.
3. Escreve o código no editor à esquerda e carrega em **Executar** (ou **F5**, ou **Ctrl+Enter**).

> O `GameMode` está definido como global em `Config/DefaultEngine.ini`, por isso funciona em qualquer mapa. Se quiseres um mapa teu: *File → Save Current Level As…* (ex.: `Content/Maps/Arena`) e define-o em *Project Settings → Maps & Modes*. Num mapa vazio o jogo cria luzes automaticamente.

## Regras do combate

- A arena é uma grelha 7×7. `x` cresce para **East**, `y` cresce para **North**.
- Em cada turno os dois programas correm até chamarem uma **ação**. Depois as ações são resolvidas **ao mesmo tempo**, por esta ordem: escudos → movimentos → ataques → curas. Por isso, afastares-te no turno certo faz o inimigo falhar o ataque.
- Cada instrução gasta **1 operação**. Começas com 100 operações por turno. Se o teu código gastar todas sem chegar a uma ação, o lutador fica **a pensar** e perde o turno (como no *Farmer Was Replaced*, código eficiente compensa). Compra Processadores para teres mais.
- **Energia**: máximo 100; recuperas +5 por turno (+15 com `wait()`, 0 enquanto bloqueias). Uma ação sem energia suficiente falha (`"tired"`).
- Se o teu programa **terminar** ou der **erro**, perdes o combate. O erro aparece na consola com o número da linha.
- O combate acaba por KO ou ao fim de 300 turnos (ganha quem tiver mais % de vida).

| Ação | Efeito | Energia | Desbloqueio |
|---|---|---|---|
| `wait()` | descansa (+15 energia) | – | – |
| `move(dir)` | anda 1 casa, devolve `True`/`False` | – | – |
| `attack()` | 10 de dano se adjacente | 15 | – |
| `block()` | bloqueia ataques; quem bate perde 15 de energia | – | Escudo |
| `dash(dir)` | anda 2 casas | 25 | Investida |
| `heavy_attack()` | 1 turno a carregar, depois 28 de dano (10 contra escudo) | 40 | Golpe pesado |
| `heal()` | 1 turno a concentrar, depois +25 de vida | 50 | Cura |
| `fireball(dir)` | projétil até 4 casas, 12 de dano (4 contra escudo) | 30 | Bola de fogo |

**Sensores** (não gastam turno): `my_x() my_y() my_hp() my_energy() enemy_x() enemy_y() enemy_hp() distance() get_tick() arena_width() arena_height() can_move(dir) opposite(dir)`, `dir_to_enemy()` [Radar], `enemy_action() enemy_energy()` [Scanner].

**Linguagem**: variáveis, `+= -= *= /=`, `+ - * / // %`, `== != < <= > >=`, `and or not`, `in`, `if/elif/else`, `while`, `break/continue/pass`, `print str int abs min max len random randint`. Desbloqueáveis: `for ... in range(n)`, `def/return/global`, listas (`[1, 2]`, `a[0]`, `a.append(x)`, `a.pop()`).

## Progressão

| # | Adversário | Vida | Ideia para vencer |
|---|---|---|---|
| 1 | Boneco de Treino | 40 | aproximar e atacar |
| 2 | Arruaceiro | 70 | gerir a energia com `wait()` |
| 3 | Tartaruga | 130 | ler o ritmo com `enemy_action()` (Scanner) |
| 4 | Fugitivo | 90 | encurralar / castigar quando está sem energia |
| 5 | Feiticeira | 90 | não ficar alinhado; usar `dash` / `block` |
| 6 | Campeão | 150 | tudo o que aprendeste |

O primeiro triunfo em cada nível dá a recompensa completa e desbloqueia o seguinte; repetir dá ouro extra (bom para comprar melhorias). O progresso e o código de cada nível são guardados automaticamente (*SaveGame* `CodeArena`).

Os inimigos estão escritos na própria FightScript, em `Source/CodeArena/Private/Core/FightLevels.cpp`. Lê-os para perceberes como se comportam.

## Arquitetura

```
CodeArena/
├── CodeArena.uproject
├── Config/                       GameMode global + mapa inicial
├── Source/CodeArena/
│   ├── Private/Core/             núcleo em C++ puro (sem Unreal), testável
│   │   ├── FightScript.*         lexer → parser (AST) → compilador (bytecode) → VM por passos
│   │   ├── FightSim.*            regras da arena: ações, sensores, resolução simultânea
│   │   └── FightLevels.*         níveis e scripts dos inimigos
│   ├── Public|Private/
│   │   ├── CodeArenaGameMode     progressão, loja, ciclo do combate, eventos → visuais/consola
│   │   ├── ArenaBoard            grelha, câmara, popups de dano, bola de fogo
│   │   ├── ArenaFighter          lutador feito de formas básicas, animações simples
│   │   ├── CodeArenaSaveGame     ouro, desbloqueios, níveis, código por nível
│   │   ├── CodeArenaContent      textos (PT), loja, ajuda, código inicial
│   │   └── UI/SCodeArenaHUD      interface em Slate: editor, consola, barras, loja, ajuda
└── Tests/                        testes do núcleo (g++/clang, sem Unreal)
```

**Porque é que a VM é "por passos"?** O motor chama `Run(MaxOps)` a cada turno. A VM devolve o controlo assim que o script chama uma ação (`move`, `attack`, ...), quando gasta as operações do turno, quando termina ou quando dá erro. Assim, um `while True:` nunca bloqueia o jogo, e o resultado da ação (por exemplo, `move()` devolve `True` se andou) é injetado na VM no turno seguinte.

### Testes do núcleo

```sh
./Tests/run_tests.sh
```

Compila o núcleo com `-Wall -Wextra -Wshadow -Werror -fno-exceptions -fno-rtti` (as mesmas restrições do Unreal) e corre os testes de linguagem e de simulação: operadores, funções, recursão, erros, bloqueios da loja e equilíbrio dos níveis (o script inicial vence os 2 primeiros níveis e perde contra a Tartaruga, e um script com Scanner vence-a).

## Como estender

- **Novo nível**: acrescenta um `MakeLevel(...)` em `FightLevels.cpp` com o script do inimigo e o nome/descrição em `CodeArenaContent.cpp` (`GetLevelText`).
- **Nova ação**: acrescenta um valor a `EAction`, regista-a em `FMatch::BindBuiltins` com `AddAction(...)` e resolve o efeito em `FMatch::Step`. Para a pôr à venda, adiciona o par `{builtin, unlock}` em `Gates` e uma entrada em `GetUnlocks()`.
- **Novo sensor**: `AddNumberSensor("nome", ...)` ou `Table.AddFunction(...)` em `BindBuiltins`.
- **Afinar o equilíbrio**: os números estão todos em `FightSim::FRules`.

Ideias para continuar: realce de sintaxe no editor (um `ITextLayoutMarshaller`), destacar a linha em execução, combates PvP entre scripts de jogadores, obstáculos na arena, modelos/animações a sério no lugar das formas básicas.
