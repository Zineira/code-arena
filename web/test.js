// Tests for the web engine (same cases as Tests/CoreTests.cpp).  Run: node web/test.js
'use strict';
const CA = require('./engine.js');

let failures = 0;
const expect = (cond, what) => { if (!cond) { failures++; console.log('  FAIL: ' + what); } };

function run(source, options) {
  let out = '';
  const table = new CA.BuiltinTable();
  CA.addStandardLibrary(table, (line) => { out += line + '\n'; }, { value: 42 });
  const res = CA.compile(source, table, options);
  if (res.error) return { out, error: 'line ' + res.error.line + ': ' + res.error.message };
  const vm = new CA.VM(res.program, table);
  for (let i = 0; i < 10000 && !vm.halted(); i++) vm.run(1000);
  return { out, error: vm.status === 'error' ? 'line ' + vm.errorLine + ': ' + vm.error : '' };
}

function output(name, src, expected) {
  const r = run(src);
  expect(!r.error, name + ' raised ' + r.error);
  expect(r.out === expected, name + ' output was:\n' + r.out + 'expected:\n' + expected);
}

function error(name, src, fragment, options) {
  const r = run(src, options);
  expect(r.error.includes(fragment), name + " expected error containing '" + fragment + "' got '" + r.error + "'");
}

console.log('Language tests');
output('arithmetic', 'print(1 + 2 * 3, 7 // 2, 7 % 3, -7 % 3, 10 / 4, -(2 + 3))\n', '7 3 1 2 2.5 -5\n');
output('strings', "s = 'ab' + \"cd\"\nprint(s, len(s), str(5) + '!', s[1])\n", 'abcd 4 5! b\n');
output('bools', 'print(1 < 2 and 3 > 4, not False, 1 == 1.0, None == None, 2 in [1, 2])\n', 'False True True True True\n');
output('if/elif/else', "for x in range(4):\n    if x == 0:\n        print('zero')\n    elif x == 1:\n        print('one')\n    else:\n        print('many', x)\n", 'zero\none\nmany 2\nmany 3\n');
output('while/break/continue', 'i = 0\ntotal = 0\nwhile True:\n    i += 1\n    if i % 2 == 0:\n        continue\n    if i > 9:\n        break\n    total += i\nprint(total)\n', '25\n');
output('for break inside function', 'def first_over(items, limit):\n    for v in items:\n        if v > limit:\n            return v\n    return -1\nprint(first_over([1, 5, 9], 4), first_over([1], 4))\nfor i in range(10):\n    if i == 3:\n        break\nprint(i)\n', '5 -1\n3\n');
output('recursion', 'def fib(n):\n    if n < 2:\n        return n\n    return fib(n - 1) + fib(n - 2)\nprint(fib(15))\n', '610\n');
output('globals', 'count = 0\ndef bump():\n    global count\n    count += 1\ndef peek():\n    return count * 10\nbump()\nbump()\nprint(count, peek())\n', '2 20\n');
output('locals shadow globals', 'x = 1\ndef f():\n    x = 5\n    return x\nprint(f(), x)\n', '5 1\n');
output('lists', 'a = [3, 1, 2]\na.append(7)\na[0] = 10\na[1] += 5\nprint(a, len(a), a[-1], a.pop(), a, min(a), max(4, 9, 2), a.index(2))\n', '[10, 6, 2] 4 7 7 [10, 6, 2] 2 9 2\n');
output('one-line blocks', "if True: print('yes')\nwhile False: pass\n", 'yes\n');
output('comments & blank lines', '# hello\n\nx = 2  # two\n\n    # indented comment\nprint(x)\n', '2\n');
output('multiline call', 'print(1,\n      2)\n', '1 2\n');
output('floats', 'print(0.1 + 0.2, 1 / 3)\n', '0.3 0.3333333333\n');

error('name error', 'print(y)\n', 'NameError');
error('zero division', 'x = 1 / 0\n', 'ZeroDivisionError');
error('bad indent', 'if True:\nprint(1)\n', 'IndentationError');
error('unexpected indent', 'x = 1\n    y = 2\n', 'unexpected indent');
error('type error', "x = 'a' + 1\n", 'TypeError');
error('arity', 'def f(a):\n    return a\nf()\n', 'takes 1');
error('recursion limit', 'def f():\n    return f()\nf()\n', 'RecursionError');
error('index', 'a = [1]\nprint(a[3])\n', 'IndexError');
error('else without if', 'else:\n    pass\n', 'without a matching');
error('accent', 'pontuação = 1\n', 'ASCII');
const locked = { allowFor: false, allowDef: false, allowLists: false };
error('locked for', 'for i in range(3):\n    pass\n', 'Locked', locked);
error('locked def', 'def f():\n    pass\n', 'Locked', locked);
error('locked lists', 'a = [1]\n', 'Locked', locked);
{
  const table = new CA.BuiltinTable();
  CA.addStandardLibrary(table, null, { value: 1 });
  const vm = new CA.VM(CA.compile('while True:\n    pass\n', table).program, table);
  expect(vm.run(50) === 'outofops' && vm.totalOps === 50, 'op budget respected');
}

const STARTER = `
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
`;

const TURTLE_SOLVER = `
while True:
    if distance() > 1:
        move(dir_to_enemy())
    elif enemy_action() == "attack" and my_energy() >= 15:
        attack()
    elif enemy_action() == "block":
        block()
    else:
        wait()
`;

const ADVANCED = `
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
`;

function fight(level, script, unlocks, ops, label) {
  const m = CA.makeMatch(level, { script, unlocks, opsPerTick: ops }, 777);
  const err = m.init();
  if (err) { console.log('  compile error', err); return -2; }
  while (!m.over) {
    m.step();
    for (const e of m.consumeEvents()) if (e.type === 'error') console.log('  runtime error', e.fighter, e.line, e.text);
  }
  console.log('  ' + label.padEnd(9) + ' vs ' + CA.LEVELS[level].enemy.padEnd(8) + ' -> winner ' + String(m.winner).padStart(2) +
    ' at tick ' + String(m.tick).padStart(3) + ' (hp ' + m.fighters[0].hp + ' vs ' + m.fighters[1].hp + ')');
  return m.winner;
}

console.log('Simulation tests');
CA.LEVELS.forEach((_, i) => expect(fight(i, ADVANCED, 'all', 1000, 'advanced') !== -2, 'level ' + i + ' compiles'));
expect(fight(0, STARTER, new Set(), 100, 'starter') === 0, 'starter beats dummy');
expect(fight(1, STARTER, new Set(), 100, 'starter') === 0, 'starter beats brawler');
expect(fight(2, STARTER, new Set(), 100, 'starter') !== 0, 'starter should not beat turtle');
expect(fight(2, TURTLE_SOLVER, new Set(['scanner', 'radar', 'block']), 100, 'solver') === 0, 'scanner script beats turtle');
{
  const m = CA.makeMatch(0, { script: 'while True:\n    block()\n' }, 1);
  expect(m.init() === null, 'locked script compiles');
  m.step();
  expect(m.consumeEvents().some((e) => e.type === 'error' && e.text.includes('Locked')), 'locked builtin reports error');
  expect(m.over && m.winner === 1, 'crashed script forfeits');
}
{
  const m = CA.makeMatch(0, { script: 'a = move(West)\nb = move(West)\nprint(a, b)\nwhile True:\n    wait()\n' }, 1);
  m.init();
  let printed = '';
  for (let i = 0; i < 4; i++) { m.step(); m.consumeEvents().forEach((e) => { if (e.type === 'print') printed += e.text; }); }
  expect(printed === 'True False', "move() results, got '" + printed + "'");
}

console.log('PvP tests');
{
  const a = { name: 'A', script: ADVANCED }, b = { name: 'B', script: STARTER };
  const r1 = CA.simulate(a, b), r2 = CA.simulate(a, b);
  expect(r1.winner === r2.winner && r1.tick === r2.tick && r1.hp[0] === r2.hp[0], 'pvp is deterministic');
  expect(r1.winner === 0, 'advanced beats starter in pvp');
  const bad = CA.simulate({ name: 'X', script: 'while True\n' }, b);
  expect(bad.winner === 1 && bad.error, 'a script that does not compile loses');
  console.log('  advanced vs starter -> winner', r1.winner, 'tick', r1.tick, 'hp', r1.hp.join('/'));
}

console.log('Battle tests');
{
  const players = [
    { name: 'Adv', script: ADVANCED }, { name: 'Start', script: STARTER },
    { name: 'Turtle', script: CA.LEVELS[2].script }, { name: 'Champ', script: CA.LEVELS[5].script },
    { name: 'Dummy', script: CA.LEVELS[0].script },
  ];
  const run = () => {
    const m = CA.makeBattle(players, 4242);
    expect(m.init() === null, 'battle compiles');
    const out = [];
    while (!m.over) {
      m.step();
      for (const e of m.consumeEvents()) if (e.type === 'eliminated') out.push(e.fighter + '@' + m.tick);
      const cells = m.fighters.filter((f) => !f.out).map((f) => f.x + ',' + f.y);
      expect(new Set(cells).size === cells.length, 'no two fighters share a cell (tick ' + m.tick + ')');
    }
    return { m, out };
  };
  const a = run(), b = run();
  expect(a.m.winner === b.m.winner && a.out.join() === b.out.join(), 'battle is deterministic');
  expect(a.m.winner >= 0 || a.m.tick === 300, 'battle ends with a winner or at the time limit');
  console.log('  5-player battle -> winner', a.m.winner >= 0 ? players[a.m.winner].name : 'draw', 'at tick', a.m.tick, 'eliminated', a.out.join(' '));
  const eight = CA.makeBattle(Array.from({ length: 8 }, (_, i) => ({ name: 'P' + i, script: CA.LEVELS[1].script })), 7);
  expect(eight.init() === null, '8 players fit');
  while (!eight.over) { eight.step(); eight.consumeEvents(); }
  console.log('  8 brawlers -> winner', eight.winner, 'at tick', eight.tick);
}

console.log(failures ? '\n' + failures + ' FAILURE(S)' : '\nALL TESTS PASSED');
process.exit(failures ? 1 : 0);
