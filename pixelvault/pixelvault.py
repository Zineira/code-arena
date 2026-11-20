#!/usr/bin/env python3
"""PixelVault: cofre local de palavras-passe e servidores SSH com um menu estilo jogo de blocos.

- Tudo fica na tua máquina: ~/.pixelvault/vault.pv, cifrado com AES-256-GCM
  e uma chave derivada da palavra-passe mestra (scrypt).
- A interface é servida só em 127.0.0.1, com um token aleatório por execução.
- Ao entrar num servidor abre-se o teu terminal com `ssh`; a palavra-passe
  é entregue ao ssh por SSH_ASKPASS (nunca aparece na linha de comandos).

Uso:  pip install cryptography   e depois   python pixelvault.py
"""
from __future__ import annotations

import argparse
import base64
import hashlib
import hmac
import json
import os
import platform
import re
import secrets
import shlex
import shutil
import socket
import subprocess
import sys
import threading
import time
import webbrowser
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

try:
    from cryptography.hazmat.primitives.ciphers.aead import AESGCM
except ImportError:  # pragma: no cover
    sys.exit("Falta a biblioteca 'cryptography'. Instala com:  pip install cryptography")

VERSION = "1.0"
HERE = Path(__file__).resolve().parent
WEB = HERE / "web"
HOME = Path(os.environ.get("PIXELVAULT_HOME") or (Path.home() / ".pixelvault"))
VAULT_FILE = HOME / "vault.pv"
RUN_DIR = HOME / "run"
IS_WIN = os.name == "nt"
IS_MAC = sys.platform == "darwin"

AAD = b"pixelvault-v1"
SCRYPT = {"n": 2 ** 15, "r": 8, "p": 1}
DEFAULT_SETTINGS = {"auto_lock_min": 10, "terminal": "auto", "clipboard_clear_s": 30, "sound": True}

HOST_RE = re.compile(r"^(?!-)[A-Za-z0-9.\-_:\[\]]{1,253}$")
USER_RE = re.compile(r"^(?!-)[A-Za-z0-9._\-\\$]{1,64}$")

LINUX_TERMINALS = [
    ("x-terminal-emulator", ["-e"]),
    ("gnome-terminal", ["--"]),
    ("konsole", ["-e"]),
    ("xfce4-terminal", ["-x"]),
    ("mate-terminal", ["-x"]),
    ("tilix", ["-e"]),
    ("kitty", []),
    ("alacritty", ["-e"]),
    ("wezterm", ["start", "--"]),
    ("foot", []),
    ("xterm", ["-e"]),
]


def b64e(b: bytes) -> str:
    return base64.b64encode(b).decode()


def b64d(s: str) -> bytes:
    return base64.b64decode(s)


def derive(master: str, salt: bytes, kdf: dict) -> bytes:
    return hashlib.scrypt(master.encode("utf-8"), salt=salt, n=kdf["n"], r=kdf["r"], p=kdf["p"],
                          maxmem=128 * 1024 * 1024, dklen=32)


class Locked(Exception):
    pass


class Vault:
    """Guarda os dados decifrados apenas em memória e apenas enquanto está desbloqueado."""

    def __init__(self, path: Path):
        self.path = path
        self.lock = threading.RLock()
        self.key: bytes | None = None
        self.salt: bytes | None = None
        self.data: dict | None = None
        self.last_activity = time.time()
        self.failed = 0
        self.blocked_until = 0.0

    # --- estado -----------------------------------------------------------
    def exists(self) -> bool:
        return self.path.exists()

    @property
    def unlocked(self) -> bool:
        return self.data is not None

    def touch(self):
        self.last_activity = time.time()

    def require(self) -> dict:
        with self.lock:
            self.check_timeout()
            if self.data is None:
                raise Locked()
            self.touch()
            return self.data

    def settings(self) -> dict:
        s = dict(DEFAULT_SETTINGS)
        if self.data:
            s.update(self.data.get("settings", {}))
        return s

    def check_timeout(self):
        if self.data is None:
            return
        mins = self.settings()["auto_lock_min"]
        if mins and time.time() - self.last_activity > mins * 60:
            self.do_lock()

    # --- ciclo de vida ----------------------------------------------------
    def create(self, master: str):
        with self.lock:
            if self.exists():
                raise ValueError("Já existe um cofre.")
            self.salt = secrets.token_bytes(16)
            self.key = derive(master, self.salt, SCRYPT)
            self.data = {"passwords": [], "servers": [], "settings": dict(DEFAULT_SETTINGS)}
            self.save()
            self.touch()

    def unlock(self, master: str) -> bool:
        with self.lock:
            now = time.time()
            if now < self.blocked_until:
                raise ValueError(f"Demasiadas tentativas. Espera {int(self.blocked_until - now) + 1} s.")
            blob = json.loads(self.path.read_text("utf-8"))
            salt = b64d(blob["salt"])
            key = derive(master, salt, blob["kdf"])
            try:
                plain = AESGCM(key).decrypt(b64d(blob["nonce"]), b64d(blob["data"]), AAD)
            except Exception:
                self.failed += 1
                if self.failed >= 3:
                    self.blocked_until = now + min(300, 5 * 2 ** (self.failed - 3))
                return False
            self.failed = 0
            self.salt, self.key = salt, key
            self.data = json.loads(plain.decode("utf-8"))
            self.data.setdefault("passwords", [])
            self.data.setdefault("servers", [])
            self.data.setdefault("settings", dict(DEFAULT_SETTINGS))
            self.touch()
            return True

    def do_lock(self):
        with self.lock:
            self.key = None
            self.data = None
            self.salt = None

    def save(self):
        with self.lock:
            assert self.key is not None and self.data is not None
            nonce = secrets.token_bytes(12)
            ct = AESGCM(self.key).encrypt(nonce, json.dumps(self.data).encode("utf-8"), AAD)
            blob = {"format": "pixelvault", "v": 1, "kdf": {"name": "scrypt", **SCRYPT},
                    "salt": b64e(self.salt), "nonce": b64e(nonce), "data": b64e(ct)}
            self.path.parent.mkdir(parents=True, exist_ok=True)
            _private_dir(self.path.parent)
            tmp = self.path.with_suffix(".tmp")
            fd = os.open(tmp, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600)
            with os.fdopen(fd, "w", encoding="utf-8") as f:
                json.dump(blob, f)
                f.flush()
                os.fsync(f.fileno())
            os.replace(tmp, self.path)

    def change_master(self, old: str, new: str) -> bool:
        with self.lock:
            self.require()
            if not hmac.compare_digest(derive(old, self.salt, SCRYPT), self.key):
                return False
            self.salt = secrets.token_bytes(16)
            self.key = derive(new, self.salt, SCRYPT)
            self.save()
            return True


def _private_dir(p: Path):
    if not IS_WIN:
        try:
            os.chmod(p, 0o700)
        except OSError:
            pass


# ---------------------------------------------------------------------------
# Tickets de uma só utilização para o SSH_ASKPASS
# ---------------------------------------------------------------------------
TICKETS: dict[str, dict] = {}
TICKETS_LOCK = threading.Lock()


def new_ticket(password: str) -> str:
    t = secrets.token_urlsafe(24)
    with TICKETS_LOCK:
        now = time.time()
        for k in [k for k, v in TICKETS.items() if v["exp"] < now]:
            del TICKETS[k]
        TICKETS[t] = {"pw": password, "exp": now + 120}
    return t


def take_ticket(t: str) -> str | None:
    with TICKETS_LOCK:
        v = TICKETS.pop(t, None)
    if v and v["exp"] >= time.time():
        return v["pw"]
    return None


# ---------------------------------------------------------------------------
# Ligações
# ---------------------------------------------------------------------------
def find_ssh() -> str | None:
    p = shutil.which("ssh")
    if not p and IS_WIN:
        cand = Path(os.environ.get("SystemRoot", r"C:\Windows")) / "System32" / "OpenSSH" / "ssh.exe"
        if cand.exists():
            p = str(cand)
    return p


def available_terminals() -> list[str]:
    if IS_WIN:
        return ["auto"]
    if IS_MAC:
        out = ["auto", "Terminal"]
        if Path("/Applications/iTerm.app").exists():
            out.append("iTerm")
        return out
    return ["auto"] + [name for name, _ in LINUX_TERMINALS if shutil.which(name)]


def askpass_program() -> str:
    """Cria (uma vez) o pequeno programa que o ssh chama para pedir a palavra-passe."""
    RUN_DIR.mkdir(parents=True, exist_ok=True)
    _private_dir(RUN_DIR)
    helper = HERE / "askpass.py"
    if IS_WIN:
        path = RUN_DIR / "askpass.cmd"
        path.write_text(f'@"{sys.executable}" "{helper}" %*\r\n', encoding="utf-8")
    else:
        path = RUN_DIR / "askpass.sh"
        path.write_text(f"#!/bin/sh\nexec {shlex.quote(sys.executable)} {shlex.quote(str(helper))} \"$@\"\n",
                        encoding="utf-8")
        os.chmod(path, 0o700)
    return str(path)


def validate_target(host: str, port, user: str):
    host = (host or "").strip()
    user = (user or "").strip()
    if not HOST_RE.match(host):
        raise ValueError("Endereço inválido.")
    if user and not USER_RE.match(user):
        raise ValueError("Nome de utilizador inválido.")
    try:
        port = int(port or 22)
    except (TypeError, ValueError):
        raise ValueError("Porta inválida.")
    if not 1 <= port <= 65535:
        raise ValueError("Porta inválida.")
    return host.strip("[]"), port, user


def build_ssh_command(srv: dict) -> list[str]:
    ssh = find_ssh()
    if not ssh:
        raise ValueError("Não encontrei o comando 'ssh'. Instala o cliente OpenSSH.")
    host, port, user = validate_target(srv.get("host"), srv.get("port"), srv.get("username"))
    args = [ssh]
    if port != 22:
        args += ["-p", str(port)]
    if user:
        args += ["-l", user]
    if srv.get("auth") == "key" and srv.get("key_path"):
        key = os.path.expanduser(srv["key_path"])
        if not Path(key).is_file():
            raise ValueError("O ficheiro da chave SSH não existe.")
        if any(c in key for c in '"%\n\r'):
            raise ValueError("Caminho da chave com caracteres não suportados.")
        args += ["-i", key, "-o", "IdentitiesOnly=yes"]
    args.append(host)
    return args


def launch_ssh(srv: dict, server_port: int, terminal: str):
    args = build_ssh_command(srv)
    env = {}
    if srv.get("auth", "password") == "password" and srv.get("password"):
        env = {
            "SSH_ASKPASS": askpass_program(),
            "SSH_ASKPASS_REQUIRE": "force",
            "DISPLAY": os.environ.get("DISPLAY", ":0"),
            "PIXELVAULT_ASKPASS_URL": f"http://127.0.0.1:{server_port}/askpass",
            "PIXELVAULT_TICKET": new_ticket(srv["password"]),
        }
    title = f"{srv.get('name') or srv.get('host')} - PixelVault"

    if IS_WIN:
        full_env = {**os.environ, **env}
        line = subprocess.list2cmdline(args)
        subprocess.Popen(f'cmd /c "title {_cmd_safe(title)} & {line} & echo. & pause"',
                         env=full_env, creationflags=subprocess.CREATE_NEW_CONSOLE)
        return

    # macOS / Linux: script temporário que se apaga a si próprio (só contém o ticket, nunca a password)
    RUN_DIR.mkdir(parents=True, exist_ok=True)
    script = RUN_DIR / f"connect-{secrets.token_hex(6)}.command"
    lines = ["#!/bin/sh", 'rm -f "$0"', f"printf '\\033]0;%s\\007' {shlex.quote(title)}"]
    lines += [f"export {k}={shlex.quote(v)}" for k, v in env.items()]
    lines += [" ".join(shlex.quote(a) for a in args),
              "echo", "printf 'Ligação terminada. Carrega em Enter para fechar.'", "read _"]
    fd = os.open(script, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o700)
    with os.fdopen(fd, "w") as f:
        f.write("\n".join(lines) + "\n")

    if IS_MAC:
        app = terminal if terminal in ("Terminal", "iTerm") else "Terminal"
        subprocess.Popen(["open", "-a", app, str(script)])
        return

    choices = LINUX_TERMINALS
    if terminal and terminal != "auto":
        choices = [t for t in LINUX_TERMINALS if t[0] == terminal] + LINUX_TERMINALS
    for name, flags in choices:
        exe = shutil.which(name)
        if exe:
            subprocess.Popen([exe, *flags, str(script)], start_new_session=True)
            return
    script.unlink(missing_ok=True)
    raise ValueError("Não encontrei nenhum terminal gráfico. Escolhe um nas Opções.")


def _cmd_safe(s: str) -> str:
    return re.sub(r'[^A-Za-z0-9 ._\-]', "", s)[:60] or "PixelVault"


def ping(host: str, port: int) -> dict:
    host, port, _ = validate_target(host, port, "")
    t0 = time.perf_counter()
    try:
        with socket.create_connection((host, port), timeout=3) as s:
            ms = int((time.perf_counter() - t0) * 1000)
            banner = ""
            s.settimeout(1.5)
            try:
                banner = s.recv(255).split(b"\n")[0].decode("utf-8", "replace").strip()
            except OSError:
                pass
        return {"ok": True, "ms": ms, "banner": banner[:80]}
    except OSError as e:
        return {"ok": False, "error": _short_err(e)}


def _short_err(e: OSError) -> str:
    if isinstance(e, socket.timeout):
        return "Tempo esgotado"
    if isinstance(e, ConnectionRefusedError):
        return "Ligação recusada"
    if isinstance(e, socket.gaierror):
        return "Endereço desconhecido"
    return "Sem ligação"


# ---------------------------------------------------------------------------
# HTTP
# ---------------------------------------------------------------------------
STATIC = {"/": ("index.html", "text/html; charset=utf-8"),
          "/pixel.ttf": ("pixel.ttf", "font/ttf")}

CSP = ("default-src 'none'; script-src 'self' 'unsafe-inline'; style-src 'self' 'unsafe-inline'; "
       "font-src 'self'; img-src 'self' data:; connect-src 'self'; base-uri 'none'; "
       "form-action 'none'; frame-ancestors 'none'")


def public_server(s: dict) -> dict:
    out = {k: v for k, v in s.items() if k != "password"}
    out["has_password"] = bool(s.get("password"))
    return out


def public_password(p: dict) -> dict:
    out = {k: v for k, v in p.items() if k != "password"}
    out["has_password"] = bool(p.get("password"))
    return out


def clean_str(v, limit=500) -> str:
    return str(v or "").strip()[:limit]


class App:
    def __init__(self, vault: Vault):
        self.vault = vault
        self.token = secrets.token_urlsafe(32)
        self.port = 0
        self.httpd: ThreadingHTTPServer | None = None

    # Cada rota recebe (body) e devolve um dict JSON.
    def route(self, method: str, path: str, body: dict):
        v = self.vault
        if method == "GET" and path == "/api/state":
            with v.lock:
                v.check_timeout()
                st = {"exists": v.exists(), "unlocked": v.unlocked, "version": VERSION,
                      "platform": platform.system(), "ssh": bool(find_ssh()),
                      "terminals": available_terminals()}
                if v.unlocked:
                    st["settings"] = v.settings()
                return st

        if path == "/api/setup":
            master = body.get("master") or ""
            if len(master) < 8:
                raise ValueError("A palavra-passe mestra precisa de pelo menos 8 caracteres.")
            v.create(master)
            return {"ok": True}

        if path == "/api/unlock":
            if not v.exists():
                raise ValueError("Ainda não existe cofre.")
            if not v.unlock(body.get("master") or ""):
                raise ValueError("Palavra-passe mestra errada.")
            return {"ok": True}

        if path == "/api/lock":
            v.do_lock()
            return {"ok": True}

        if path == "/api/quit":
            v.do_lock()
            threading.Thread(target=self.httpd.shutdown, daemon=True).start()
            return {"ok": True}

        data = v.require()

        if path == "/api/settings":
            s = data.setdefault("settings", dict(DEFAULT_SETTINGS))
            if "auto_lock_min" in body:
                s["auto_lock_min"] = max(0, min(240, int(body["auto_lock_min"])))
            if "clipboard_clear_s" in body:
                s["clipboard_clear_s"] = max(0, min(600, int(body["clipboard_clear_s"])))
            if "terminal" in body:
                t = clean_str(body["terminal"], 40)
                s["terminal"] = t if t in available_terminals() else "auto"
            if "sound" in body:
                s["sound"] = bool(body["sound"])
            v.save()
            return {"settings": v.settings()}

        if path == "/api/master":
            new = body.get("new") or ""
            if len(new) < 8:
                raise ValueError("A nova palavra-passe mestra precisa de pelo menos 8 caracteres.")
            if not v.change_master(body.get("old") or "", new):
                raise ValueError("A palavra-passe mestra atual está errada.")
            return {"ok": True}

        # --- palavras-passe (um jogador) ---
        if path == "/api/passwords" and method == "GET":
            return {"items": [public_password(p) for p in data["passwords"]]}
        if path == "/api/passwords" and method == "POST":
            return {"item": public_password(self.upsert(data["passwords"], body, self.fill_password))}
        if path == "/api/passwords/delete":
            data["passwords"] = [p for p in data["passwords"] if p["id"] != body.get("id")]
            v.save()
            return {"ok": True}
        if path == "/api/passwords/reveal":
            p = self.find(data["passwords"], body.get("id"))
            return {"password": p.get("password", ""), "username": p.get("username", "")}

        # --- servidores (multijogador) ---
        if path == "/api/servers" and method == "GET":
            return {"items": [public_server(s) for s in data["servers"]]}
        if path == "/api/servers" and method == "POST":
            return {"item": public_server(self.upsert(data["servers"], body, self.fill_server))}
        if path == "/api/servers/delete":
            data["servers"] = [s for s in data["servers"] if s["id"] != body.get("id")]
            v.save()
            return {"ok": True}
        if path == "/api/servers/move":
            lst = data["servers"]
            i = next((k for k, s in enumerate(lst) if s["id"] == body.get("id")), None)
            j = None if i is None else i + (1 if body.get("dir") == "down" else -1)
            if i is not None and 0 <= j < len(lst):
                lst[i], lst[j] = lst[j], lst[i]
                v.save()
            return {"ok": True}
        if path == "/api/servers/ping":
            s = self.find(data["servers"], body.get("id"))
            return ping(s["host"], s.get("port", 22))
        if path == "/api/servers/connect":
            s = self.find(data["servers"], body.get("id"))
            launch_ssh(s, self.port, v.settings()["terminal"])
            s["last_used"] = int(time.time())
            v.save()
            return {"ok": True}
        if path == "/api/direct":
            srv = {"host": body.get("host"), "port": body.get("port"), "username": body.get("username"),
                   "auth": "agent", "name": body.get("host")}
            launch_ssh(srv, self.port, v.settings()["terminal"])
            return {"ok": True}

        raise LookupError(path)

    @staticmethod
    def find(lst, id_):
        for x in lst:
            if x["id"] == id_:
                return x
        raise ValueError("Entrada não encontrada.")

    def upsert(self, lst, body, fill):
        id_ = body.get("id")
        if id_:
            item = self.find(lst, id_)
        else:
            item = {"id": secrets.token_hex(8), "created": int(time.time())}
        fill(item, body)
        item["updated"] = int(time.time())
        if not id_:
            lst.append(item)
        self.vault.save()
        return item

    @staticmethod
    def fill_password(item, b):
        item["name"] = clean_str(b.get("name"), 100) or "Sem nome"
        item["username"] = clean_str(b.get("username"), 200)
        item["url"] = clean_str(b.get("url"), 500)
        item["notes"] = str(b.get("notes") or "")[:5000]
        if b.get("password") is not None:  # None = manter a atual
            item["password"] = str(b["password"])[:1000]

    @staticmethod
    def fill_server(item, b):
        host, port, user = validate_target(b.get("host"), b.get("port"), b.get("username"))
        item["name"] = clean_str(b.get("name"), 100) or host
        item["host"], item["port"], item["username"] = host, port, user
        auth = b.get("auth") if b.get("auth") in ("password", "key", "agent") else "password"
        item["auth"] = auth
        item["key_path"] = clean_str(b.get("key_path"), 500) if auth == "key" else ""
        if auth != "password":
            item.pop("password", None)
        elif b.get("password") is not None:
            item["password"] = str(b["password"])[:1000]


def make_handler(app: App):
    class Handler(BaseHTTPRequestHandler):
        server_version = "PixelVault"
        sys_version = ""

        def log_message(self, *a):  # nada de logs com caminhos/tickets
            pass

        def _host_ok(self) -> bool:
            h = self.headers.get("Host", "")
            return h in (f"127.0.0.1:{app.port}", f"localhost:{app.port}")

        def _send(self, code: int, payload, ctype="application/json; charset=utf-8"):
            raw = payload if isinstance(payload, bytes) else json.dumps(payload).encode("utf-8")
            self.send_response(code)
            self.send_header("Content-Type", ctype)
            self.send_header("Content-Length", str(len(raw)))
            self.send_header("Cache-Control", "no-store")
            self.send_header("Content-Security-Policy", CSP)
            self.send_header("X-Content-Type-Options", "nosniff")
            self.send_header("X-Frame-Options", "DENY")
            self.send_header("Referrer-Policy", "no-referrer")
            self.end_headers()
            self.wfile.write(raw)

        def _body(self) -> dict:
            n = int(self.headers.get("Content-Length") or 0)
            if n > 1_000_000:
                raise ValueError("Pedido demasiado grande.")
            if not n:
                return {}
            return json.loads(self.rfile.read(n).decode("utf-8"))

        def do_GET(self):
            self._handle("GET")

        def do_POST(self):
            self._handle("POST")

        def _handle(self, method):
            if not self._host_ok():
                return self._send(403, {"error": "Host não permitido."})
            path = self.path.split("?", 1)[0]

            if method == "GET" and path in STATIC:
                name, ctype = STATIC[path]
                return self._send(200, (WEB / name).read_bytes(), ctype)

            if path == "/askpass" and method == "POST":
                try:
                    pw = take_ticket(self._body().get("ticket", ""))
                except Exception:
                    pw = None
                if pw is None:
                    return self._send(404, {"error": "ticket"})
                return self._send(200, {"password": pw})

            if not path.startswith("/api/"):
                return self._send(404, {"error": "Não encontrado."})
            if not hmac.compare_digest(self.headers.get("X-PV-Token", ""), app.token):
                return self._send(401, {"error": "Token inválido. Abre o link que aparece na consola."})
            if method == "POST" and not self.headers.get("Content-Type", "").startswith("application/json"):
                return self._send(415, {"error": "JSON obrigatório."})
            try:
                body = self._body() if method == "POST" else {}
                self._send(200, app.route(method, path, body))
            except Locked:
                self._send(423, {"error": "O cofre está bloqueado.", "locked": True})
            except LookupError:
                self._send(404, {"error": "Não encontrado."})
            except ValueError as e:
                self._send(400, {"error": str(e)})
            except Exception as e:  # pragma: no cover
                self._send(500, {"error": f"Erro interno: {type(e).__name__}"})

    return Handler


def auto_lock_loop(vault: Vault):
    while True:
        time.sleep(15)
        with vault.lock:
            vault.check_timeout()


def main():
    ap = argparse.ArgumentParser(description="PixelVault: cofre local com menu de jogo de blocos")
    ap.add_argument("--port", type=int, default=0, help="porta local (0 = aleatória)")
    ap.add_argument("--no-browser", action="store_true", help="não abrir o browser automaticamente")
    args = ap.parse_args()

    HOME.mkdir(parents=True, exist_ok=True)
    _private_dir(HOME)
    for old in RUN_DIR.glob("connect-*") if RUN_DIR.exists() else []:
        old.unlink(missing_ok=True)

    app = App(Vault(VAULT_FILE))
    httpd = ThreadingHTTPServer(("127.0.0.1", args.port), make_handler(app))
    httpd.daemon_threads = True
    app.httpd = httpd
    app.port = httpd.server_address[1]
    threading.Thread(target=auto_lock_loop, args=(app.vault,), daemon=True).start()

    url = f"http://127.0.0.1:{app.port}/#t={app.token}"
    print(f"PixelVault {VERSION}  ·  cofre: {VAULT_FILE}")
    print(f"Abre no browser:  {url}")
    print("Ctrl+C para sair.")
    if not args.no_browser:
        threading.Timer(0.4, lambda: webbrowser.open(url)).start()
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        app.vault.do_lock()
        print("\nCofre bloqueado. Até à próxima!")


if __name__ == "__main__":
    main()
