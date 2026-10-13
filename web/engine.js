// Code Arena web engine: FightScript (a tiny Python-like language compiled to
// bytecode and run by a step-able VM) plus the turn-based arena simulation.
// JavaScript port of Source/CodeArena/Private/Core (same rules and levels).
// Works in the browser (window.CodeArena) and in Node (require).

(function (root) {
  'use strict';

  // ------------------------------------------------------------------
  // Values: None = null, bool, number, str, list = Array,
  // functions / builtins = tagged objects.
  // ------------------------------------------------------------------

  const UNDEF = { kind: 'undef' };
  const MAX_LIST = 100000;
  const MAX_STACK = 100000;
  const MAX_DEPTH = 200;

  const isFn = (v) => v !== null && typeof v === 'object' && v.kind === 'fn';
  const isBuiltin = (v) => v !== null && typeof v === 'object' && v.kind === 'builtin';

  function typeName(v) {
    if (v === null) return 'NoneType';
    if (v === UNDEF) return 'undefined';
    if (typeof v === 'boolean') return 'bool';
    if (typeof v === 'number') return 'number';
    if (typeof v === 'string') return 'str';
    if (Array.isArray(v)) return 'list';
    if (isFn(v)) return 'function';
    if (isBuiltin(v)) return 'builtin_function';
    return '?';
  }

  function truthy(v) {
    if (v === null || v === UNDEF) return false;
    if (typeof v === 'boolean') return v;
    if (typeof v === 'number') return v !== 0;
    if (typeof v === 'string') return v.length > 0;
    if (Array.isArray(v)) return v.length > 0;
    return true;
  }

  const numeric = (v) => typeof v === 'number' || typeof v === 'boolean';
  const asNum = (v) => (typeof v === 'boolean' ? (v ? 1 : 0) : v);

  function equals(a, b) {
    if (numeric(a) && numeric(b)) return asNum(a) === asNum(b);
    if (typeName(a) !== typeName(b)) return false;
    if (Array.isArray(a)) {
      if (a === b) return true;
      if (a.length !== b.length) return false;
      for (let i = 0; i < a.length; i++) if (!equals(a[i], b[i])) return false;
      return true;
    }
    if (isFn(a) || isBuiltin(a)) return a.index === b.index;
    return a === b;
  }

  function formatNumber(n) {
    if (Number.isFinite(n) && Math.abs(n) < 1e15 && Number.isInteger(n)) return String(n);
    if (!Number.isFinite(n)) return n > 0 ? 'inf' : n < 0 ? '-inf' : 'nan';
    return String(parseFloat(n.toPrecision(10)));
  }

  function display(v) {
    if (v === null) return 'None';
    if (typeof v === 'boolean') return v ? 'True' : 'False';
    if (typeof v === 'number') return formatNumber(v);
    if (typeof v === 'string') return v;
    if (Array.isArray(v)) return '[' + v.map((x) => (typeof x === 'string' ? "'" + x + "'" : display(x))).join(', ') + ']';
    if (isFn(v)) return '<function>';
    if (isBuiltin(v)) return '<builtin function>';
    return '<undefined>';
  }

  // ------------------------------------------------------------------
  // Builtins
  // ------------------------------------------------------------------

  const OK = 0, SUSPEND = 1, ERR = 2;

  class BuiltinTable {
    constructor() { this.items = []; this.byName = new Map(); }
    addFunction(name, minArgs, maxArgs, fn) {
      this.byName.set(name, this.items.length);
      this.items.push({ name, minArgs, maxArgs, fn, locked: false, lockedMessage: '' });
    }
    addConstant(name, value) {
      this.byName.set(name, this.items.length);
      this.items.push({ name, constant: value, fn: null, locked: false });
    }
    find(name) { return this.byName.has(name) ? this.byName.get(name) : -1; }
    setLocked(name, message) {
      const i = this.find(name);
      if (i >= 0) { this.items[i].locked = true; this.items[i].lockedMessage = message; }
    }
  }

  function nextRandom(state) {
    let x = state.value || 0x9e3779b9;
    x ^= x << 13; x >>>= 0;
    x ^= x >>> 17;
    x ^= x << 5; x >>>= 0;
    state.value = x;
    return x;
  }

  function wholeArg(v, fname) {
    if (typeof v === 'boolean') return { value: v ? 1 : 0 };
    if (typeof v !== 'number' || !Number.isInteger(v)) {
      return { error: 'TypeError: ' + fname + '() expects whole numbers, got ' + typeName(v) };
    }
    return { value: v };
  }

  function minMax(args, isMax) {
    const name = isMax ? 'max' : 'min';
    let items = args;
    if (args.length === 1) {
      if (!Array.isArray(args[0])) return { error: 'TypeError: ' + name + '() expects a list or several numbers' };
      items = args[0];
    }
    if (items.length === 0) return { error: 'ValueError: ' + name + '() of an empty list' };
    let best = null;
    for (const item of items) {
      if (typeof item !== 'number') return { error: 'TypeError: ' + name + '() only works with numbers' };
      if (best === null || (isMax ? item > best : item < best)) best = item;
    }
    return { value: best };
  }

  // Each builtin fn(args) returns { status, value?, error? }.
  function addStandardLibrary(table, print, rng) {
    const ok = (value) => ({ status: OK, value });
    const err = (error) => ({ status: ERR, error });

    table.addFunction('print', 0, 16, (args) => { if (print) print(args.map(display).join(' ')); return ok(null); });
    table.addFunction('range', 1, 3, (args) => {
      let start = 0, stop, step = 1;
      const parts = args.map((a) => wholeArg(a, 'range'));
      const bad = parts.find((p) => p.error);
      if (bad) return err(bad.error);
      if (args.length === 1) stop = parts[0].value;
      else { start = parts[0].value; stop = parts[1].value; if (args.length === 3) step = parts[2].value; }
      if (step === 0) return err('ValueError: range() step must not be zero');
      const out = [];
      for (let i = start; step > 0 ? i < stop : i > stop; i += step) {
        if (out.length >= MAX_LIST) return err('MemoryError: range() is too big');
        out.push(i);
      }
      return ok(out);
    });
    table.addFunction('len', 1, 1, (args) => {
      const v = args[0];
      if (Array.isArray(v) || typeof v === 'string') return ok(v.length);
      return err("TypeError: object of type '" + typeName(v) + "' has no len()");
    });
    table.addFunction('str', 1, 1, (args) => ok(display(args[0])));
    table.addFunction('int', 1, 1, (args) => {
      const v = args[0];
      if (typeof v === 'number') return ok(Math.trunc(v));
      if (typeof v === 'boolean') return ok(v ? 1 : 0);
      if (typeof v === 'string') {
        const t = v.trim();
        if (t !== '' && Number.isFinite(Number(t))) return ok(Math.trunc(Number(t)));
        return err("ValueError: invalid literal for int(): '" + v + "'");
      }
      return err("TypeError: int() can't convert " + typeName(v));
    });
    table.addFunction('abs', 1, 1, (args) => typeof args[0] === 'number' ? ok(Math.abs(args[0]))
      : err("TypeError: bad operand type for abs(): '" + typeName(args[0]) + "'"));
    table.addFunction('min', 1, 16, (args) => { const r = minMax(args, false); return r.error ? err(r.error) : ok(r.value); });
    table.addFunction('max', 1, 16, (args) => { const r = minMax(args, true); return r.error ? err(r.error) : ok(r.value); });
    table.addFunction('random', 0, 0, () => ok((nextRandom(rng) % 1000000) / 1000000));
    table.addFunction('randint', 2, 2, (args) => {
      const a = wholeArg(args[0], 'randint'), b = wholeArg(args[1], 'randint');
      if (a.error || b.error) return err(a.error || b.error);
      if (b.value < a.value) return err('ValueError: randint(a, b) needs a <= b');
      return ok(a.value + (nextRandom(rng) % (b.value - a.value + 1)));
    });
  }

  // ------------------------------------------------------------------
  // Lexer
  // ------------------------------------------------------------------

  const KEYWORDS = new Set(['if', 'elif', 'else', 'while', 'for', 'in', 'def', 'return', 'break', 'continue',
    'pass', 'and', 'or', 'not', 'True', 'False', 'None', 'global']);
  const TWO_CHAR_OPS = ['==', '!=', '<=', '>=', '//', '+=', '-=', '*=', '/='];
  const SINGLE_OPS = '+-*/%<>=()[],:.';

  class CompileError extends Error {
    constructor(line, message) { super(message); this.line = line; }
  }

  function tokenize(src) {
    const out = [];
    const indents = [0];
    let pos = 0, line = 1, paren = 0, lineStart = true;
    const push = (kind, text, num) => out.push({ kind, text: text || '', num: num || 0, line });
    const fail = (msg) => { throw new CompileError(line, msg); };
    const isDigit = (c) => c >= '0' && c <= '9';
    const isIdent = (c) => (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c === '_';

    for (;;) {
      if (lineStart && paren === 0) {
        let width = 0, p = pos;
        while (p < src.length && (src[p] === ' ' || src[p] === '\t')) { width += src[p] === '\t' ? 4 : 1; p++; }
        if (p >= src.length) { pos = p; break; }
        const first = src[p];
        if (first === '\n' || first === '\r' || first === '#') {
          while (p < src.length && src[p] !== '\n') p++;
          if (p < src.length) { p++; line++; }
          pos = p;
          continue;
        }
        pos = p;
        lineStart = false;
        if (width > indents[indents.length - 1]) { indents.push(width); push('indent'); }
        else {
          while (width < indents[indents.length - 1]) { indents.pop(); push('dedent'); }
          if (width !== indents[indents.length - 1]) fail('IndentationError: unindent does not match any outer indentation level');
        }
      }
      if (pos >= src.length) break;
      const c = src[pos];
      if (c === '\n') {
        if (paren === 0 && out.length && out[out.length - 1].kind !== 'newline') push('newline');
        line++; pos++; lineStart = true; continue;
      }
      if (c === ' ' || c === '\t' || c === '\r') { pos++; continue; }
      if (c === '#') { while (pos < src.length && src[pos] !== '\n') pos++; continue; }
      if (c === '\\' && src[pos + 1] === '\n') { pos += 2; line++; continue; }
      if (isDigit(c) || (c === '.' && isDigit(src[pos + 1] || ''))) {
        const start = pos;
        while (pos < src.length && (isDigit(src[pos]) || src[pos] === '.')) pos++;
        const text = src.slice(start, pos);
        if (!/^(\d+\.?\d*|\.\d+)$/.test(text)) fail("SyntaxError: invalid number '" + text + "'");
        push('number', text, parseFloat(text));
        continue;
      }
      if (isIdent(c)) {
        const start = pos;
        while (pos < src.length && (isIdent(src[pos]) || isDigit(src[pos]))) pos++;
        const text = src.slice(start, pos);
        push(KEYWORDS.has(text) ? 'keyword' : 'name', text);
        continue;
      }
      if (c === '"' || c === "'") {
        pos++;
        let text = '', closed = false;
        while (pos < src.length) {
          const s = src[pos];
          if (s === c) { pos++; closed = true; break; }
          if (s === '\n') break;
          if (s === '\\' && pos + 1 < src.length) {
            const e = src[pos + 1];
            pos += 2;
            text += { n: '\n', t: '\t', '\\': '\\', '"': '"', "'": "'" }[e] || '\\' + e;
            continue;
          }
          text += s; pos++;
        }
        if (!closed) fail('SyntaxError: unterminated string');
        push('string', text);
        continue;
      }
      const two = src.slice(pos, pos + 2);
      if (TWO_CHAR_OPS.includes(two)) { push('op', two); pos += 2; continue; }
      if (SINGLE_OPS.includes(c)) {
        if (c === '(' || c === '[') paren++;
        else if ((c === ')' || c === ']') && paren > 0) paren--;
        push('op', c); pos++; continue;
      }
      if (c.charCodeAt(0) >= 128) fail('SyntaxError: only plain ASCII characters are allowed in code (no accents)');
      fail("SyntaxError: invalid character '" + c + "'");
    }
    if (out.length && out[out.length - 1].kind !== 'newline') push('newline');
    while (indents.length > 1) { indents.pop(); push('dedent'); }
    push('end');
    return out;
  }

  // ------------------------------------------------------------------
  // Parser -> AST
  // ------------------------------------------------------------------

  const COMPARE_OPS = { '==': 'eq', '!=': 'ne', '<': 'lt', '<=': 'le', '>': 'gt', '>=': 'ge' };
  const AUG_OPS = { '+=': 'add', '-=': 'sub', '*=': 'mul', '/=': 'div' };

  function parse(tokens) {
    let pos = 0;
    const peek = (o = 0) => tokens[Math.min(pos + o, tokens.length - 1)];
    const advance = () => { const t = tokens[pos]; if (pos < tokens.length - 1) pos++; return t; };
    const isOp = (t, o = 0) => peek(o).kind === 'op' && peek(o).text === t;
    const isKw = (t, o = 0) => peek(o).kind === 'keyword' && peek(o).text === t;
    const fail = (msg) => { throw new CompileError(peek().line, msg); };
    const describe = (t) => ({ newline: 'end of line', indent: 'indentation', dedent: 'end of block', end: 'end of file', string: 'string' }[t.kind] || "'" + t.text + "'");
    const expectOp = (t) => { if (!isOp(t)) fail("SyntaxError: expected '" + t + "' but found " + describe(peek())); advance(); };
    const expectNewline = () => {
      if (peek().kind === 'newline') { advance(); return; }
      if (peek().kind === 'end') return;
      fail('SyntaxError: unexpected ' + describe(peek()) + ' (missing operator or new line?)');
    };

    function program() {
      const body = [];
      while (peek().kind !== 'end') body.push(statement());
      return body;
    }

    function statement() {
      const t = peek();
      if (t.kind === 'indent') fail('IndentationError: unexpected indent');
      if (t.kind === 'keyword') {
        if (t.text === 'if') return ifStmt();
        if (t.text === 'while') { const line = advance().line; const cond = expr(); return { kind: 'while', line, cond, body: block() }; }
        if (t.text === 'for') return forStmt();
        if (t.text === 'def') return defStmt();
        if (t.text === 'elif' || t.text === 'else') fail("SyntaxError: '" + t.text + "' without a matching 'if' (check the indentation)");
      }
      const s = simple();
      expectNewline();
      return s;
    }

    function block() {
      expectOp(':');
      if (peek().kind !== 'newline') { const s = simple(); expectNewline(); return [s]; }
      advance();
      if (peek().kind !== 'indent') fail('IndentationError: expected an indented block');
      advance();
      const body = [];
      while (peek().kind !== 'dedent' && peek().kind !== 'end') body.push(statement());
      if (peek().kind === 'dedent') advance();
      return body;
    }

    function ifStmt() {
      const line = advance().line;
      const cond = expr();
      const body = block();
      let orelse = [];
      if (isKw('elif')) orelse = [ifStmt()];
      else if (isKw('else')) { advance(); orelse = block(); }
      return { kind: 'if', line, cond, body, orelse };
    }

    function forStmt() {
      const line = advance().line;
      if (peek().kind !== 'name') fail("SyntaxError: expected a variable name after 'for'");
      const name = advance().text;
      if (!isKw('in')) fail("SyntaxError: expected 'in' in for loop");
      advance();
      const iter = expr();
      return { kind: 'for', line, name, iter, body: block() };
    }

    function defStmt() {
      const line = advance().line;
      if (peek().kind !== 'name') fail("SyntaxError: expected a function name after 'def'");
      const name = advance().text;
      expectOp('(');
      const params = [];
      while (!isOp(')')) {
        if (peek().kind !== 'name') fail('SyntaxError: expected a parameter name');
        params.push(advance().text);
        if (isOp(',')) advance();
        else if (!isOp(')')) fail("SyntaxError: expected ',' or ')' in parameter list");
      }
      advance();
      return { kind: 'def', line, name, params, body: block() };
    }

    function simple() {
      const t = peek(), line = t.line;
      if (t.kind === 'keyword') {
        if (t.text === 'pass' || t.text === 'break' || t.text === 'continue') { advance(); return { kind: t.text, line }; }
        if (t.text === 'return') {
          advance();
          const value = peek().kind !== 'newline' && peek().kind !== 'end' ? expr() : null;
          return { kind: 'return', line, value };
        }
        if (t.text === 'global') {
          advance();
          const names = [];
          do {
            if (peek().kind !== 'name') fail("SyntaxError: expected a name after 'global'");
            names.push(advance().text);
          } while (isOp(',') && advance());
          return { kind: 'global', line, names };
        }
      }
      const e = expr();
      const assignable = e.kind === 'name' || e.kind === 'index';
      if (isOp('=')) {
        advance();
        if (!assignable) fail('SyntaxError: can only assign to a variable or a list element');
        return { kind: 'assign', line, target: e, value: expr() };
      }
      if (peek().kind === 'op' && AUG_OPS[peek().text]) {
        const op = AUG_OPS[advance().text];
        if (!assignable) fail('SyntaxError: can only assign to a variable or a list element');
        return { kind: 'augassign', line, op, target: e, value: expr() };
      }
      return { kind: 'expr', line, value: e };
    }

    function expr() { return orExpr(); }

    function orExpr() {
      let left = andExpr();
      while (isKw('or')) { const line = advance().line; left = { kind: 'or', line, left, right: andExpr() }; }
      return left;
    }

    function andExpr() {
      let left = notExpr();
      while (isKw('and')) { const line = advance().line; left = { kind: 'and', line, left, right: notExpr() }; }
      return left;
    }

    function notExpr() {
      if (isKw('not')) { const line = advance().line; return { kind: 'not', line, operand: notExpr() }; }
      return comparison();
    }

    function comparison() {
      let left = arith();
      for (;;) {
        const line = peek().line;
        let op = null;
        if (peek().kind === 'op' && COMPARE_OPS[peek().text]) op = COMPARE_OPS[advance().text];
        else if (isKw('in')) { advance(); op = 'in'; }
        else if (isKw('not') && isKw('in', 1)) { advance(); advance(); op = 'notin'; }
        if (!op) return left;
        left = { kind: 'binary', line, op, left, right: arith() };
      }
    }

    function arith() {
      let left = term();
      while (isOp('+') || isOp('-')) {
        const t = advance();
        left = { kind: 'binary', line: t.line, op: t.text === '+' ? 'add' : 'sub', left, right: term() };
      }
      return left;
    }

    function term() {
      let left = unary();
      while (isOp('*') || isOp('/') || isOp('//') || isOp('%')) {
        const t = advance();
        const op = { '*': 'mul', '/': 'div', '//': 'floordiv', '%': 'mod' }[t.text];
        left = { kind: 'binary', line: t.line, op, left, right: unary() };
      }
      return left;
    }

    function unary() {
      if (isOp('-')) { const line = advance().line; return { kind: 'neg', line, operand: unary() }; }
      if (isOp('+')) { advance(); return unary(); }
      return postfix();
    }

    function args() {
      const out = [];
      while (!isOp(')')) {
        out.push(expr());
        if (isOp(',')) advance();
        else if (!isOp(')')) fail("SyntaxError: expected ',' or ')' in call, found " + describe(peek()));
      }
      advance();
      return out;
    }

    function postfix() {
      let e = atom();
      for (;;) {
        if (isOp('(')) { const line = advance().line; e = { kind: 'call', line, callee: e, args: args() }; }
        else if (isOp('[')) { const line = advance().line; const key = expr(); expectOp(']'); e = { kind: 'index', line, obj: e, key }; }
        else if (isOp('.')) {
          const line = advance().line;
          if (peek().kind !== 'name') fail("SyntaxError: expected a method name after '.'");
          const name = advance().text;
          expectOp('(');
          e = { kind: 'method', line, obj: e, name, args: args() };
        } else return e;
      }
    }

    function atom() {
      const t = peek(), line = t.line;
      if (t.kind === 'number') { advance(); return { kind: 'const', line, value: t.num }; }
      if (t.kind === 'string') { advance(); return { kind: 'const', line, value: t.text }; }
      if (t.kind === 'name') { advance(); return { kind: 'name', line, name: t.text }; }
      if (t.kind === 'keyword') {
        if (t.text === 'True' || t.text === 'False') { advance(); return { kind: 'const', line, value: t.text === 'True' }; }
        if (t.text === 'None') { advance(); return { kind: 'const', line, value: null }; }
        fail("SyntaxError: unexpected keyword '" + t.text + "'");
      }
      if (isOp('(')) { advance(); const e = expr(); expectOp(')'); return e; }
      if (isOp('[')) {
        advance();
        const items = [];
        while (!isOp(']')) {
          items.push(expr());
          if (isOp(',')) advance();
          else if (!isOp(']')) fail("SyntaxError: expected ',' or ']' in list");
        }
        advance();
        return { kind: 'list', line, items };
      }
      fail('SyntaxError: unexpected ' + describe(t));
    }

    return program();
  }

  // ------------------------------------------------------------------
  // Compiler -> bytecode. Instructions are [op, a, b, line].
  // ------------------------------------------------------------------

  function compileAst(stmts, builtins, options) {
    const program = { functions: [{ name: '<main>', numParams: 0, numLocals: 0, code: [] }], constants: [], globalNames: [] };
    const globalSlots = new Map();
    const moduleNames = new Set();
    let cur = { proto: 0, isModule: true, locals: new Map(), declaredGlobals: new Set(), loops: [] };
    let lastLine = 1;

    const code = () => program.functions[cur.proto].code;
    const here = () => code().length;
    const emit = (op, a = 0, b = 0, line = lastLine) => { code().push([op, a, b, line]); lastLine = line; return code().length - 1; };
    const patch = (at, target) => { code()[at][1] = target; };
    const constant = (v) => { program.constants.push(v); return program.constants.length - 1; };
    const fail = (line, msg) => { throw new CompileError(line, msg); };
    const slot = (name) => {
      if (!globalSlots.has(name)) { globalSlots.set(name, program.globalNames.length); program.globalNames.push(name); }
      return globalSlots.get(name);
    };
    const checkLists = (line) => {
      if (!options.allowLists) fail(line, 'Locked: lists are not unlocked yet (buy them in the shop)');
    };

    function collectModule(list) {
      for (const s of list) {
        if ((s.kind === 'assign' || s.kind === 'augassign') && s.target.kind === 'name') moduleNames.add(s.target.name);
        else if (s.kind === 'for') { moduleNames.add(s.name); collectModule(s.body); }
        else if (s.kind === 'if') { collectModule(s.body); collectModule(s.orelse); }
        else if (s.kind === 'while') collectModule(s.body);
        else if (s.kind === 'def') { moduleNames.add(s.name); collectGlobals(s.body); }
      }
    }
    function collectGlobals(list) {
      for (const s of list) {
        if (s.kind === 'global') s.names.forEach((n) => moduleNames.add(n));
        if (s.body) collectGlobals(s.body);
        if (s.orelse) collectGlobals(s.orelse);
      }
    }
    function collectLocals(list, ctx, order) {
      for (const s of list) if (s.kind === 'global') s.names.forEach((n) => ctx.declaredGlobals.add(n));
      for (const s of list) {
        if ((s.kind === 'assign' || s.kind === 'augassign') && s.target.kind === 'name') order.push(s.target.name);
        else if (s.kind === 'for') { order.push(s.name); collectLocals(s.body, ctx, order); }
        else if (s.kind === 'if') { collectLocals(s.body, ctx, order); collectLocals(s.orelse, ctx, order); }
        else if (s.kind === 'while') collectLocals(s.body, ctx, order);
      }
    }

    function load(name, line) {
      if (!cur.isModule && cur.locals.has(name)) return emit('loadl', cur.locals.get(name), 0, line);
      if (!moduleNames.has(name) && !cur.declaredGlobals.has(name)) {
        const b = builtins.find(name);
        if (b >= 0) return emit('loadb', b, 0, line);
      }
      emit('loadg', slot(name), 0, line);
    }
    function store(name, line) {
      if (!cur.isModule && cur.locals.has(name)) return emit('storel', cur.locals.get(name), 0, line);
      emit('storeg', slot(name), 0, line);
    }

    function block(list) { for (const s of list) stmt(s); }

    function stmt(s) {
      const line = s.line;
      switch (s.kind) {
        case 'expr': expression(s.value); emit('pop', 0, 0, line); return;
        case 'assign':
          if (s.target.kind === 'name') { expression(s.value); store(s.target.name, line); return; }
          checkLists(line);
          expression(s.target.obj); expression(s.target.key); expression(s.value);
          emit('storeidx', 0, 0, line);
          return;
        case 'augassign':
          if (s.target.kind === 'name') {
            load(s.target.name, line); expression(s.value); emit('bin', s.op, 0, line); store(s.target.name, line);
            return;
          }
          checkLists(line);
          expression(s.target.obj); expression(s.target.key);
          emit('dup2', 0, 0, line); emit('index', 0, 0, line);
          expression(s.value); emit('bin', s.op, 0, line); emit('storeidx', 0, 0, line);
          return;
        case 'if': {
          expression(s.cond);
          const jElse = emit('jf', 0, 0, line);
          block(s.body);
          if (!s.orelse.length) { patch(jElse, here()); return; }
          const jEnd = emit('jmp', 0, 0, line);
          patch(jElse, here());
          block(s.orelse);
          patch(jEnd, here());
          return;
        }
        case 'while': {
          const start = here();
          expression(s.cond);
          const jEnd = emit('jf', 0, 0, line);
          cur.loops.push({ isFor: false, cont: start, breaks: [] });
          block(s.body);
          emit('jmp', start, 0, line);
          const loop = cur.loops.pop();
          patch(jEnd, here());
          loop.breaks.forEach((b) => patch(b, here()));
          return;
        }
        case 'for': {
          if (!options.allowFor) fail(line, "Locked: 'for' loops are not unlocked yet (buy them in the shop)");
          expression(s.iter);
          emit('getiter', 0, 0, line);
          const top = emit('foriter', 0, 0, line);
          store(s.name, line);
          cur.loops.push({ isFor: true, cont: top, breaks: [] });
          block(s.body);
          emit('jmp', top, 0, line);
          const loop = cur.loops.pop();
          patch(top, here());
          loop.breaks.forEach((b) => patch(b, here()));
          return;
        }
        case 'break':
        case 'continue': {
          if (!cur.loops.length) fail(line, "SyntaxError: '" + s.kind + "' outside loop");
          const loop = cur.loops[cur.loops.length - 1];
          if (s.kind === 'continue') { emit('jmp', loop.cont, 0, line); return; }
          if (loop.isFor) { emit('pop', 0, 0, line); emit('pop', 0, 0, line); }
          loop.breaks.push(emit('jmp', 0, 0, line));
          return;
        }
        case 'def': return def(s);
        case 'return':
          if (cur.isModule) fail(line, "SyntaxError: 'return' outside function");
          if (s.value) expression(s.value); else emit('const', constant(null), 0, line);
          emit('ret', 0, 0, line);
          return;
        case 'pass':
        case 'global':
          return;
      }
    }

    function def(s) {
      const line = s.line;
      if (!options.allowDef) fail(line, "Locked: 'def' (functions) is not unlocked yet (buy it in the shop)");
      if (!cur.isModule) fail(line, 'SyntaxError: functions can only be defined at the top level');
      const proto = program.functions.length;
      program.functions.push({ name: s.name, numParams: s.params.length, numLocals: 0, code: [] });
      const ctx = { proto, isModule: false, locals: new Map(), declaredGlobals: new Set(), loops: [] };
      const order = s.params.slice();
      collectLocals(s.body, ctx, order);
      for (const n of order) if (!ctx.declaredGlobals.has(n) && !ctx.locals.has(n)) ctx.locals.set(n, ctx.locals.size);
      program.functions[proto].numLocals = ctx.locals.size;
      const outer = cur;
      cur = ctx;
      block(s.body);
      emit('const', constant(null), 0, lastLine);
      emit('ret', 0, 0, lastLine);
      cur = outer;
      emit('const', constant({ kind: 'fn', index: proto }), 0, line);
      store(s.name, line);
    }

    function expression(e) {
      const line = e.line;
      switch (e.kind) {
        case 'const': emit('const', constant(e.value), 0, line); return;
        case 'name': load(e.name, line); return;
        case 'list': checkLists(line); e.items.forEach(expression); emit('mklist', e.items.length, 0, line); return;
        case 'index': checkLists(line); expression(e.obj); expression(e.key); emit('index', 0, 0, line); return;
        case 'call': expression(e.callee); e.args.forEach(expression); emit('call', e.args.length, 0, line); return;
        case 'method': checkLists(line); expression(e.obj); e.args.forEach(expression); emit('method', constant(e.name), e.args.length, line); return;
        case 'neg': expression(e.operand); emit('neg', 0, 0, line); return;
        case 'not': expression(e.operand); emit('not', 0, 0, line); return;
        case 'binary': expression(e.left); expression(e.right); emit('bin', e.op, 0, line); return;
        case 'and':
        case 'or': {
          expression(e.left);
          const j = emit(e.kind === 'and' ? 'jfop' : 'jtop', 0, 0, line);
          expression(e.right);
          patch(j, here());
          return;
        }
      }
    }

    collectModule(stmts);
    block(stmts);
    emit('halt', 0, 0, lastLine);
    return program;
  }

  // Returns { program } or { error: { line, message } }.
  function compile(source, builtins, options) {
    const opts = Object.assign({ allowFor: true, allowDef: true, allowLists: true }, options || {});
    try {
      return { program: compileAst(parse(tokenize(source)), builtins, opts) };
    } catch (e) {
      if (e instanceof CompileError) return { error: { line: e.line, message: e.message } };
      throw e;
    }
  }

  // ------------------------------------------------------------------
  // VM
  // ------------------------------------------------------------------

  class VMError extends Error {}

  function listIndex(key, size) {
    if (typeof key !== 'number' || !Number.isInteger(key)) throw new VMError('TypeError: indices must be whole numbers, not ' + typeName(key));
    let i = key < 0 ? key + size : key;
    if (i < 0 || i >= size) throw new VMError('IndexError: index out of range');
    return i;
  }

  function binary(op, l, r) {
    const nums = numeric(l) && numeric(r);
    const a = asNum(l), b = asNum(r);
    const typeError = (sym) => new VMError('TypeError: unsupported operand types for ' + sym + ": '" + typeName(l) + "' and '" + typeName(r) + "'");
    switch (op) {
      case 'add':
        if (nums) return a + b;
        if (typeof l === 'string' && typeof r === 'string') return l + r;
        if (Array.isArray(l) && Array.isArray(r)) {
          if (l.length + r.length > MAX_LIST) throw new VMError('MemoryError: list is too big');
          return l.concat(r);
        }
        if (typeof l === 'string' || typeof r === 'string') throw new VMError('TypeError: can only join text with text (use str(x) to convert a number)');
        throw typeError('+');
      case 'sub': if (!nums) throw typeError('-'); return a - b;
      case 'mul': if (!nums) throw typeError('*'); return a * b;
      case 'div':
        if (!nums) throw typeError('/');
        if (b === 0) throw new VMError('ZeroDivisionError: division by zero');
        return a / b;
      case 'floordiv':
        if (!nums) throw typeError('//');
        if (b === 0) throw new VMError('ZeroDivisionError: division by zero');
        return Math.floor(a / b);
      case 'mod': {
        if (!nums) throw typeError('%');
        if (b === 0) throw new VMError('ZeroDivisionError: modulo by zero');
        let m = a % b;
        if (m !== 0 && (m < 0) !== (b < 0)) m += b;
        return m;
      }
      case 'eq': return equals(l, r);
      case 'ne': return !equals(l, r);
      case 'lt': case 'le': case 'gt': case 'ge': {
        let cmp;
        if (nums) cmp = a < b ? -1 : a > b ? 1 : 0;
        else if (typeof l === 'string' && typeof r === 'string') cmp = l < r ? -1 : l > r ? 1 : 0;
        else throw new VMError("TypeError: can't compare '" + typeName(l) + "' with '" + typeName(r) + "'");
        return op === 'lt' ? cmp < 0 : op === 'le' ? cmp <= 0 : op === 'gt' ? cmp > 0 : cmp >= 0;
      }
      case 'in': case 'notin': {
        let found;
        if (Array.isArray(r)) found = r.some((x) => equals(x, l));
        else if (typeof r === 'string' && typeof l === 'string') found = r.includes(l);
        else throw new VMError("TypeError: argument of type '" + typeName(r) + "' is not a list");
        return op === 'in' ? found : !found;
      }
    }
    throw new VMError('internal error: unknown operator');
  }

  function callMethod(obj, name, args) {
    if (!Array.isArray(obj)) throw new VMError("AttributeError: '" + typeName(obj) + "' object has no method '" + name + "'");
    switch (name) {
      case 'append':
        if (args.length !== 1) throw new VMError('TypeError: append() takes exactly 1 argument');
        if (obj.length >= MAX_LIST) throw new VMError('MemoryError: list is too big');
        obj.push(args[0]);
        return null;
      case 'pop': {
        if (args.length > 1) throw new VMError('TypeError: pop() takes at most 1 argument');
        if (!obj.length) throw new VMError('IndexError: pop from empty list');
        const i = args.length ? listIndex(args[0], obj.length) : obj.length - 1;
        return obj.splice(i, 1)[0];
      }
      case 'insert': {
        if (args.length !== 2) throw new VMError('TypeError: insert() takes exactly 2 arguments');
        if (typeof args[0] !== 'number') throw new VMError('TypeError: insert() index must be a number');
        let i = Math.trunc(args[0]);
        if (i < 0) i += obj.length;
        obj.splice(Math.max(0, Math.min(obj.length, i)), 0, args[1]);
        return null;
      }
      case 'index': {
        if (args.length !== 1) throw new VMError('TypeError: index() takes exactly 1 argument');
        const i = obj.findIndex((x) => equals(x, args[0]));
        if (i < 0) throw new VMError('ValueError: value is not in list');
        return i;
      }
    }
    throw new VMError("AttributeError: 'list' object has no method '" + name + "'");
  }

  class VM {
    // status: 'ready' | 'yielded' | 'outofops' | 'finished' | 'error'
    constructor(program, builtins) {
      this.program = program;
      this.builtins = builtins;
      this.stack = [];
      this.frames = [{ fn: 0, ip: 0, base: 0 }];
      this.globals = program.globalNames.map(() => UNDEF);
      this.status = 'ready';
      this.error = '';
      this.errorLine = 0;
      this.line = 0;
      this.totalOps = 0;
    }

    halted() { return this.status === 'finished' || this.status === 'error'; }

    setYieldResult(v) { if (this.stack.length) this.stack[this.stack.length - 1] = v; }

    fail(msg) { this.status = 'error'; this.error = msg; this.errorLine = this.line; return this.status; }

    run(maxOps) {
      if (this.halted()) return this.status;
      this.status = 'ready';
      try {
        return this.loop(maxOps);
      } catch (e) {
        if (e instanceof VMError) return this.fail(e.message);
        throw e;
      }
    }

    loop(maxOps) {
      const { program, builtins, stack } = this;
      let ops = 0;
      while (ops < maxOps) {
        const frame = this.frames[this.frames.length - 1];
        const ins = program.functions[frame.fn].code[frame.ip++];
        const op = ins[0], a = ins[1];
        this.line = ins[3];
        ops++; this.totalOps++;
        if (stack.length > MAX_STACK) return this.fail('MemoryError: stack overflow');
        switch (op) {
          case 'const': stack.push(program.constants[a]); break;
          case 'loadl': stack.push(stack[frame.base + a]); break;
          case 'storel': stack[frame.base + a] = stack.pop(); break;
          case 'loadg': {
            const v = this.globals[a];
            if (v === UNDEF) return this.fail("NameError: name '" + program.globalNames[a] + "' is not defined");
            stack.push(v);
            break;
          }
          case 'storeg': this.globals[a] = stack.pop(); break;
          case 'loadb': {
            const b = builtins.items[a];
            stack.push(b.fn ? { kind: 'builtin', index: a } : b.constant);
            break;
          }
          case 'pop': stack.pop(); break;
          case 'dup2': { const n = stack.length; stack.push(stack[n - 2], stack[n - 1]); break; }
          case 'bin': { const r = stack.pop(); const l = stack.pop(); stack.push(binary(a, l, r)); break; }
          case 'neg': {
            const v = stack[stack.length - 1];
            if (typeof v !== 'number') return this.fail("TypeError: bad operand type for unary -: '" + typeName(v) + "'");
            stack[stack.length - 1] = -v;
            break;
          }
          case 'not': stack[stack.length - 1] = !truthy(stack[stack.length - 1]); break;
          case 'jmp': frame.ip = a; break;
          case 'jf': if (!truthy(stack.pop())) frame.ip = a; break;
          case 'jfop': if (!truthy(stack[stack.length - 1])) frame.ip = a; else stack.pop(); break;
          case 'jtop': if (truthy(stack[stack.length - 1])) frame.ip = a; else stack.pop(); break;
          case 'mklist': stack.push(stack.splice(stack.length - a, a)); break;
          case 'index': {
            const key = stack.pop(); const c = stack.pop();
            if (Array.isArray(c)) stack.push(c[listIndex(key, c.length)]);
            else if (typeof c === 'string') stack.push(c[listIndex(key, c.length)]);
            else return this.fail("TypeError: '" + typeName(c) + "' object is not subscriptable");
            break;
          }
          case 'storeidx': {
            const v = stack.pop(); const key = stack.pop(); const c = stack.pop();
            if (!Array.isArray(c)) return this.fail("TypeError: '" + typeName(c) + "' object does not support item assignment");
            c[listIndex(key, c.length)] = v;
            break;
          }
          case 'call': {
            const calleeAt = stack.length - a - 1;
            const callee = stack[calleeAt];
            if (isFn(callee)) {
              const target = program.functions[callee.index];
              if (a !== target.numParams) {
                return this.fail('TypeError: ' + target.name + '() takes ' + target.numParams + ' argument(s) but ' + a + ' were given');
              }
              if (this.frames.length >= MAX_DEPTH) return this.fail('RecursionError: maximum recursion depth exceeded');
              const base = calleeAt + 1;
              while (stack.length < base + target.numLocals) stack.push(null);
              this.frames.push({ fn: callee.index, ip: 0, base });
              break;
            }
            if (isBuiltin(callee)) {
              const b = builtins.items[callee.index];
              if (b.locked) return this.fail(b.lockedMessage || 'Locked: ' + b.name + '() is not unlocked yet');
              if (a < b.minArgs || a > b.maxArgs) {
                const expected = b.minArgs === b.maxArgs ? String(b.minArgs) : b.minArgs + ' to ' + b.maxArgs;
                return this.fail('TypeError: ' + b.name + '() takes ' + expected + ' argument(s) but ' + a + ' were given');
              }
              const args = stack.slice(calleeAt + 1);
              const res = b.fn(args);
              if (res.status === ERR) return this.fail(res.error);
              stack.length = calleeAt;
              stack.push(res.value === undefined ? null : res.value);
              if (res.status === SUSPEND) { this.status = 'yielded'; return this.status; }
              break;
            }
            return this.fail("TypeError: '" + typeName(callee) + "' object is not callable");
          }
          case 'method': {
            const argc = ins[2];
            const args = stack.splice(stack.length - argc, argc);
            const obj = stack.pop();
            stack.push(callMethod(obj, program.constants[a], args));
            break;
          }
          case 'ret': {
            const result = stack.pop();
            const base = frame.base;
            this.frames.pop();
            stack.length = base - 1;
            stack.push(result);
            break;
          }
          case 'getiter': {
            const it = stack.pop();
            if (!Array.isArray(it)) return this.fail("TypeError: '" + typeName(it) + "' object is not iterable (use range(n) to loop n times)");
            stack.push(it.slice(), 0);
            break;
          }
          case 'foriter': {
            const n = stack.length;
            const i = stack[n - 1], items = stack[n - 2];
            if (i < items.length) { stack[n - 1] = i + 1; stack.push(items[i]); }
            else { stack.length = n - 2; frame.ip = a; }
            break;
          }
          case 'halt': this.status = 'finished'; return this.status;
        }
      }
      this.status = 'outofops';
      return this.status;
    }
  }

  // ------------------------------------------------------------------
  // Arena simulation
  // ------------------------------------------------------------------

  const DIRS = ['North', 'East', 'South', 'West'];
  const DELTA = { North: [0, 1], East: [1, 0], South: [0, -1], West: [-1, 0] };
  const OPPOSITE = { North: 'South', South: 'North', East: 'West', West: 'East' };

  const RULES = {
    attackDamage: 10, heavyDamage: 28, heavyBlockedDamage: 10,
    fireballDamage: 12, fireballBlockedDamage: 4, fireballRange: 4, healAmount: 25,
    attackCost: 15, dashCost: 25, heavyCost: 40, healCost: 50, fireballCost: 30, parryEnergyLoss: 15,
    maxEnergy: 100, energyRegen: 5, waitBonusRegen: 10,
  };

  // Builtins sold in the shop: builtin name -> unlock id.
  const GATES = {
    block: 'block', dash: 'dash', enemy_action: 'scanner', enemy_energy: 'scanner',
    dir_to_enemy: 'radar', heavy_attack: 'heavy', heal: 'heal', fireball: 'fireball',
  };

  function costOf(action) {
    return { attack: RULES.attackCost, dash: RULES.dashCost, heavy: RULES.heavyCost, heal: RULES.healCost, fireball: RULES.fireballCost }[action] || 0;
  }

  class Match {
    // config: { width, height, maxTicks, seed, fighters: [{name, script, maxHp, opsPerTick, unlocks:Set|'all'}], start: [x0,y0,x1,y1] }
    // Returns via init(): null on success or { fighter, line, message }.
    constructor(config) {
      this.config = config;
      this.tick = 0;
      this.over = false;
      this.winner = -1;
      this.events = [];
      this.rng = { value: (config.seed >>> 0) || 1 };
      this.fighters = [];
    }

    unlocked(i, id) {
      const u = this.config.fighters[i].unlocks;
      return u === 'all' || (u && u.has(id));
    }

    init() {
      const c = this.config;
      for (let i = 0; i < 2; i++) {
        const fc = c.fighters[i];
        const f = {
          name: fc.name, x: c.start[i * 2], y: c.start[i * 2 + 1],
          hp: fc.maxHp, maxHp: fc.maxHp, energy: RULES.maxEnergy, ops: fc.opsPerTick,
          facing: i === 0 ? 'East' : 'West', last: 'idle', blocking: false, halted: false,
          requested: 'idle', dir: 'North', pending: 'idle', busy: 0,
        };
        this.fighters.push(f);
      }
      for (let i = 0; i < 2; i++) {
        const table = this.bind(i);
        const res = compile(c.fighters[i].script, table, {
          allowFor: this.unlocked(i, 'for'), allowDef: this.unlocked(i, 'def'), allowLists: this.unlocked(i, 'lists'),
        });
        if (res.error) return { fighter: i, line: res.error.line, message: res.error.message };
        this.fighters[i].vm = new VM(res.program, table);
      }
      return null;
    }

    event(type, fighter, extra) { this.events.push(Object.assign({ type, fighter }, extra || {})); }

    consumeEvents() { const e = this.events; this.events = []; return e; }

    inBounds(x, y) { return x >= 0 && y >= 0 && x < this.config.width && y < this.config.height; }

    distance() { const [a, b] = this.fighters; return Math.abs(a.x - b.x) + Math.abs(a.y - b.y); }

    dirTowards(fx, fy, tx, ty) {
      const dx = tx - fx, dy = ty - fy;
      if (Math.abs(dx) >= Math.abs(dy) && dx !== 0) return dx > 0 ? 'East' : 'West';
      return dy >= 0 ? 'North' : 'South';
    }

    bind(i) {
      const me = () => this.fighters[i];
      const them = () => this.fighters[1 - i];
      const t = new BuiltinTable();
      const ok = (value) => ({ status: OK, value });
      const parseDir = (v) => (typeof v === 'string' && DELTA[v] ? v : null);
      const dirError = (v) => ({ status: ERR, error: 'ValueError: expected a direction (North, East, South or West), got ' + display(v) });

      addStandardLibrary(t, (line) => this.event('print', i, { text: line }), this.rng);
      DIRS.forEach((d) => t.addConstant(d, d));

      const sensors = {
        my_x: () => me().x, my_y: () => me().y, my_hp: () => me().hp, my_energy: () => me().energy,
        enemy_x: () => them().x, enemy_y: () => them().y, enemy_hp: () => them().hp, enemy_energy: () => them().energy,
        distance: () => this.distance(), get_tick: () => this.tick,
        arena_width: () => this.config.width, arena_height: () => this.config.height,
        enemy_action: () => them().last,
        dir_to_enemy: () => this.dirTowards(me().x, me().y, them().x, them().y),
      };
      for (const [name, fn] of Object.entries(sensors)) t.addFunction(name, 0, 0, () => ok(fn()));

      t.addFunction('opposite', 1, 1, (args) => { const d = parseDir(args[0]); return d ? ok(OPPOSITE[d]) : dirError(args[0]); });
      t.addFunction('can_move', 1, 1, (args) => {
        const d = parseDir(args[0]);
        if (!d) return dirError(args[0]);
        const nx = me().x + DELTA[d][0], ny = me().y + DELTA[d][1];
        return ok(this.inBounds(nx, ny) && !(nx === them().x && ny === them().y));
      });

      const action = (name, act, needsDir) => {
        const n = needsDir ? 1 : 0;
        t.addFunction(name, n, n, (args) => {
          let d = me().facing;
          if (needsDir) { d = parseDir(args[0]); if (!d) return dirError(args[0]); }
          me().requested = act;
          me().dir = d;
          return { status: SUSPEND, value: null };
        });
      };
      action('wait', 'wait', false);
      action('move', 'move', true);
      action('attack', 'attack', false);
      action('block', 'block', false);
      action('dash', 'dash', true);
      action('heavy_attack', 'heavy', false);
      action('heal', 'heal', false);
      action('fireball', 'fireball', true);

      for (const [name, id] of Object.entries(GATES)) {
        if (!this.unlocked(i, id)) t.setLocked(name, 'Locked: ' + name + "() is not unlocked yet (buy '" + id + "' in the shop)");
      }
      return t;
    }

    decide(i, results, hasResult) {
      const f = this.fighters[i];
      hasResult[i] = false;
      results[i] = null;
      if (f.busy > 0) {
        f.busy--;
        if (f.busy === 0) { f.requested = f.pending; f.pending = 'idle'; hasResult[i] = true; }
        else f.requested = f.pending === 'heavy' ? 'charge' : 'channel';
        return;
      }
      if (f.halted) { f.requested = 'idle'; return; }
      const status = f.vm.run(f.ops);
      if (status === 'outofops') { f.requested = 'think'; return; }
      if (status === 'finished') { f.requested = 'idle'; f.halted = true; this.event('ended', i); return; }
      if (status === 'error') {
        f.requested = 'idle'; f.halted = true;
        this.event('error', i, { text: f.vm.error, line: f.vm.errorLine });
        return;
      }
      hasResult[i] = true;
      const cost = costOf(f.requested);
      if (f.energy < cost) {
        this.event('tired', i, { text: f.requested, amount: cost });
        f.requested = 'tired';
        results[i] = false;
        return;
      }
      f.energy -= cost;
      if (f.requested === 'heavy' || f.requested === 'heal') {
        f.pending = f.requested;
        f.busy = 1;
        f.requested = f.requested === 'heavy' ? 'charge' : 'channel';
        hasResult[i] = false;
        this.event('charging', i, { text: f.requested });
      }
    }

    step() {
      if (this.over) return;
      this.tick++;
      const F = this.fighters;
      const results = [null, null], hasResult = [false, false];
      this.decide(0, results, hasResult);
      this.decide(1, results, hasResult);
      const acts = [F[0].requested, F[1].requested];

      // 1) blocking
      for (let i = 0; i < 2; i++) F[i].blocking = acts[i] === 'block';

      // 2) movement
      const wants = [false, false];
      const targets = [[F[0].x, F[0].y], [F[1].x, F[1].y]];
      for (let i = 0; i < 2; i++) {
        if (acts[i] !== 'move' && acts[i] !== 'dash') continue;
        const f = F[i], e = F[1 - i];
        f.facing = f.dir;
        const [dx, dy] = DELTA[f.dir];
        let tx = f.x, ty = f.y;
        for (let s = 0; s < (acts[i] === 'dash' ? 2 : 1); s++) {
          const nx = tx + dx, ny = ty + dy;
          if (!this.inBounds(nx, ny) || (nx === e.x && ny === e.y)) break;
          tx = nx; ty = ny;
        }
        targets[i] = [tx, ty];
        wants[i] = tx !== f.x || ty !== f.y;
      }
      if (wants[0] && wants[1]) {
        const same = targets[0][0] === targets[1][0] && targets[0][1] === targets[1][1];
        const swap = targets[0][0] === F[1].x && targets[0][1] === F[1].y && targets[1][0] === F[0].x && targets[1][1] === F[0].y;
        if (swap) { wants[0] = wants[1] = false; }
        else if (same) { wants[(nextRandom(this.rng) >>> 8) & 1] = false; }
      }
      for (let pass = 0; pass < 2; pass++) {
        for (let i = 0; i < 2; i++) {
          const o = 1 - i;
          const ox = wants[o] ? targets[o][0] : F[o].x, oy = wants[o] ? targets[o][1] : F[o].y;
          if (wants[i] && targets[i][0] === ox && targets[i][1] === oy) wants[i] = false;
        }
      }
      for (let i = 0; i < 2; i++) {
        if (acts[i] === 'move' || acts[i] === 'dash') {
          if (wants[i]) { F[i].x = targets[i][0]; F[i].y = targets[i][1]; this.event('moved', i, { text: acts[i] }); }
          results[i] = wants[i];
        }
      }

      // 3) attacks (after movement, so stepping away dodges)
      const damage = [0, 0];
      const dist = this.distance();
      for (let i = 0; i < 2; i++) {
        const f = F[i], e = F[1 - i];
        if (acts[i] === 'attack' || acts[i] === 'heavy') {
          f.facing = this.dirTowards(f.x, f.y, e.x, e.y);
          const heavy = acts[i] === 'heavy';
          if (dist !== 1) { this.event('missed', i, { text: acts[i] }); results[i] = false; continue; }
          if (e.blocking) {
            const through = heavy ? RULES.heavyBlockedDamage : 0;
            damage[1 - i] += through;
            if (!heavy) f.energy = Math.max(0, f.energy - RULES.parryEnergyLoss);
            this.event('blocked', i, { target: 1 - i, amount: through, text: acts[i] });
            results[i] = heavy;
            continue;
          }
          const amount = heavy ? RULES.heavyDamage : RULES.attackDamage;
          damage[1 - i] += amount;
          this.event('hit', i, { target: 1 - i, amount, text: acts[i] });
          results[i] = true;
        } else if (acts[i] === 'fireball') {
          f.facing = f.dir;
          const [dx, dy] = DELTA[f.dir];
          let cx = f.x, cy = f.y, hit = false;
          for (let s = 0; s < RULES.fireballRange; s++) {
            const nx = cx + dx, ny = cy + dy;
            if (!this.inBounds(nx, ny)) break;
            cx = nx; cy = ny;
            if (cx === e.x && cy === e.y) { hit = true; break; }
          }
          let amount = 0;
          if (hit) { amount = e.blocking ? RULES.fireballBlockedDamage : RULES.fireballDamage; damage[1 - i] += amount; }
          this.event('fireball', i, { target: hit ? 1 - i : -1, amount, x: cx, y: cy, blocked: hit && e.blocking });
          results[i] = hit;
        }
      }
      F[0].hp -= damage[0];
      F[1].hp -= damage[1];

      // 4) heals, energy, results
      for (let i = 0; i < 2; i++) {
        const f = F[i];
        if (acts[i] === 'heal' && f.hp > 0) {
          const before = f.hp;
          f.hp = Math.min(f.maxHp, f.hp + RULES.healAmount);
          this.event('healed', i, { amount: f.hp - before });
          results[i] = true;
        }
        if (acts[i] !== 'block') f.energy += RULES.energyRegen;
        if (acts[i] === 'wait') f.energy += RULES.waitBonusRegen;
        f.energy = Math.min(f.energy, RULES.maxEnergy);
        f.last = acts[i];
        if (hasResult[i]) f.vm.setYieldResult(results[i]);
      }

      // 5) end conditions
      const dead0 = F[0].hp <= 0, dead1 = F[1].hp <= 0;
      if (dead0 || dead1) return this.finish(dead0 && dead1 ? -1 : dead0 ? 1 : 0, 'knockout');
      if (F[0].halted || F[1].halted) {
        return this.finish(F[0].halted && F[1].halted ? -1 : F[0].halted ? 1 : 0, F[0].halted && F[1].halted ? 'both scripts stopped' : 'script stopped');
      }
      if (this.tick >= this.config.maxTicks) {
        const r0 = F[0].hp / F[0].maxHp, r1 = F[1].hp / F[1].maxHp;
        this.finish(r0 === r1 ? -1 : r0 > r1 ? 0 : 1, 'time limit');
      }
    }

    finish(winner, reason) {
      this.over = true;
      this.winner = winner;
      this.event('over', Math.max(winner, 0), { target: winner, text: reason });
    }
  }

  // ------------------------------------------------------------------
  // Levels (enemies are FightScript programs)
  // ------------------------------------------------------------------

  const LEVELS = [
    { enemy: 'Dummy', hp: 40, reward: 40, repeat: 5, script:
`# Training dummy: it just stands there.
while True:
    wait()
` },
    { enemy: 'Brawler', hp: 70, reward: 60, repeat: 10, script:
`# Walks straight at you and swings when it can.
while True:
    if distance() > 1:
        move(dir_to_enemy())
    elif my_energy() >= 15:
        attack()
    else:
        wait()
` },
    { enemy: 'Turtle', hp: 130, reward: 90, repeat: 15, script:
`# Fixed rhythm when adjacent: block, block, attack, rest.
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
` },
    { enemy: 'Runner', hp: 90, reward: 120, repeat: 20, script:
`# Hit and run: strikes once, then dashes away to recover.
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
` },
    { enemy: 'Witch', hp: 90, reward: 160, repeat: 25, script:
`# Keeps her distance and throws fireballs along rows and columns.
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
` },
    { enemy: 'Champion', hp: 150, reward: 250, repeat: 40, script:
`# Reads your moves, punishes tired opponents and heals when safe.
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
` },
  ].map((l) => Object.assign({ width: 7, height: 7, maxTicks: 300, enemyOps: 1000 }, l));

  function makeMatch(levelIndex, player, seed) {
    const level = LEVELS[levelIndex];
    const mid = Math.floor(level.height / 2);
    return new Match({
      width: level.width, height: level.height, maxTicks: level.maxTicks, seed,
      fighters: [
        Object.assign({ name: 'Player', maxHp: 100, opsPerTick: 100, unlocks: new Set() }, player),
        { name: level.enemy, script: level.script, maxHp: level.hp, opsPerTick: level.enemyOps, unlocks: 'all' },
      ],
      start: [1, mid, level.width - 2, mid],
    });
  }

  const api = { compile, VM, BuiltinTable, addStandardLibrary, display, Match, makeMatch, LEVELS, RULES, OK, SUSPEND, ERR };
  if (typeof module !== 'undefined' && module.exports) module.exports = api;
  else root.CodeArena = api;
})(typeof window !== 'undefined' ? window : globalThis);
