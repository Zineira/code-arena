"""Testes do PixelVault:  python test_pixelvault.py"""
import json
import os
import subprocess
import sys
import tempfile
import threading
import urllib.error
import urllib.request
from pathlib import Path

TMP = tempfile.mkdtemp(prefix="pv-test-")
os.environ["PIXELVAULT_HOME"] = TMP
sys.path.insert(0, str(Path(__file__).resolve().parent))
import pixelvault as pv  # noqa: E402

app = pv.App(pv.Vault(pv.VAULT_FILE))
httpd = pv.ThreadingHTTPServer(("127.0.0.1", 0), pv.make_handler(app))
app.httpd, app.port = httpd, httpd.server_address[1]
threading.Thread(target=httpd.serve_forever, daemon=True).start()
BASE = f"http://127.0.0.1:{app.port}"
NOPROXY = urllib.request.build_opener(urllib.request.ProxyHandler({}))


def call(path, body=None, token=True, host=None):
    headers = {"X-PV-Token": app.token} if token else {}
    if host:
        headers["Host"] = host
    data = None
    if body is not None:
        data = json.dumps(body).encode()
        headers["Content-Type"] = "application/json"
    req = urllib.request.Request(BASE + path, data=data, headers=headers, method="POST" if body is not None else "GET")
    try:
        with NOPROXY.open(req, timeout=10) as r:
            return r.status, json.loads(r.read() or b"{}")
    except urllib.error.HTTPError as e:
        return e.code, json.loads(e.read() or b"{}")


def check(cond, msg):
    print(("ok   " if cond else "FAIL ") + msg)
    if not cond:
        global failed
        failed += 1


failed = 0
check(call("/api/state", token=False)[0] == 401, "API sem token é recusada")
check(call("/api/state", host="evil.example:80")[0] == 403, "Host estranho é recusado (DNS rebinding)")
check(call("/api/state")[1]["exists"] is False, "sem cofre no início")
check(call("/api/setup", {"master": "curta"})[0] == 400, "mestra curta recusada")
check(call("/api/setup", {"master": "correct horse battery"})[0] == 200, "cofre criado")
s, r = call("/api/servers", {"name": "Casa", "host": "192.168.1.10", "port": 2222, "username": "pi",
                             "auth": "password", "password": "segredo-ssh"})
check(s == 200 and "password" not in r["item"] and r["item"]["has_password"], "servidor guardado sem devolver a password")
srv_id = r["item"]["id"]
check(call("/api/servers", {"host": "-oProxyCommand=x", "auth": "agent"})[0] == 400, "host com '-' recusado")
check(call("/api/servers", {"host": "a.b", "username": "x;rm", "auth": "agent"})[0] == 400, "utilizador com ';' recusado")
s, r = call("/api/passwords", {"name": "Email", "username": "eu@x.pt", "password": "p@ss-1234"})
pw_id = r["item"]["id"]
check(call("/api/passwords/reveal", {"id": pw_id})[1]["password"] == "p@ss-1234", "revelar palavra-passe")

raw = Path(TMP, "vault.pv").read_text()
check("segredo-ssh" not in raw and "p@ss-1234" not in raw and "Casa" not in raw, "ficheiro do cofre não tem texto simples")
if os.name != "nt":
    check(oct(os.stat(Path(TMP, "vault.pv")).st_mode & 0o777) == "0o600", "ficheiro do cofre com permissões 600")

# editar servidor sem password => mantém a anterior
call("/api/servers", {"id": srv_id, "name": "Casa 2", "host": "192.168.1.10", "port": 2222, "username": "pi",
                      "auth": "password", "password": None})
check(app.vault.data["servers"][0]["password"] == "segredo-ssh", "editar sem password mantém a atual")

# comando ssh
pv.find_ssh = lambda: "/usr/bin/ssh"
cmd = pv.build_ssh_command(app.vault.data["servers"][0])
check(cmd == ["/usr/bin/ssh", "-p", "2222", "-l", "pi", "192.168.1.10"], f"comando ssh: {cmd}")

# askpass: o ticket entrega a password uma vez, só a programas locais
t = pv.new_ticket("segredo-ssh")
env = {**os.environ, "PIXELVAULT_ASKPASS_URL": BASE + "/askpass", "PIXELVAULT_TICKET": t}
out = subprocess.run([sys.executable, "askpass.py", "pi@host's password: "], env=env, capture_output=True,
                     text=True, cwd=Path(__file__).parent, stdin=subprocess.DEVNULL, timeout=20)
check(out.stdout == "segredo-ssh\n", "askpass devolve a password do ticket")
check(pv.take_ticket(t) is None, "ticket só funciona uma vez")

# lock / unlock
call("/api/lock", {})
check(call("/api/servers")[0] == 423, "bloqueado => 423")
check(call("/api/unlock", {"master": "errada!!"})[0] == 400, "mestra errada recusada")
check(call("/api/unlock", {"master": "correct horse battery"})[0] == 200, "desbloquear")
check(call("/api/servers")[1]["items"][0]["name"] == "Casa 2", "dados persistem após desbloquear")
check(call("/api/master", {"old": "correct horse battery", "new": "nova mestra 123"})[0] == 200, "mudar mestra")
call("/api/lock", {})
check(call("/api/unlock", {"master": "nova mestra 123"})[0] == 200, "desbloquear com a nova mestra")

# ping a uma porta local aberta (o próprio servidor)
r = pv.ping("127.0.0.1", app.port)
check(r["ok"], f"ping local: {r}")

print("\n" + ("Tudo certo!" if not failed else f"{failed} falha(s)"))
sys.exit(1 if failed else 0)
