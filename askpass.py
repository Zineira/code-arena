#!/usr/bin/env python3
"""Programa SSH_ASKPASS do PixelVault.

O ssh chama isto com a pergunta em argv[1]. Para a primeira pergunta de palavra-passe
vai buscar a password ao PixelVault (127.0.0.1) com um ticket de uso único. Qualquer outra
pergunta (impressão digital do anfitrião, segunda tentativa, código 2FA...) é feita
diretamente no terminal, para seres tu a responder.
"""
import getpass
import json
import os
import sys
import urllib.request


def ask_terminal(prompt: str) -> str:
    secret = not ("yes/no" in prompt or "(yes/no" in prompt or "fingerprint" in prompt.lower())
    try:
        if os.name == "nt":
            if secret:
                return getpass.getpass(prompt)
            with open("CONIN$", "r") as cin, open("CONOUT$", "w") as cout:
                cout.write(prompt)
                cout.flush()
                return cin.readline().rstrip("\r\n")
        with open("/dev/tty", "r+") as tty:
            if secret:
                return getpass.getpass(prompt, stream=tty)
            tty.write(prompt)
            tty.flush()
            return tty.readline().rstrip("\n")
    except (OSError, EOFError, KeyboardInterrupt):
        return ""


def from_vault() -> str | None:
    url = os.environ.get("PIXELVAULT_ASKPASS_URL", "")
    ticket = os.environ.pop("PIXELVAULT_TICKET", "")
    if not ticket or not url.startswith("http://127.0.0.1:"):
        return None
    opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))  # nunca usar proxy
    req = urllib.request.Request(url, data=json.dumps({"ticket": ticket}).encode(),
                                 headers={"Content-Type": "application/json"}, method="POST")
    try:
        with opener.open(req, timeout=5) as r:
            return json.loads(r.read().decode("utf-8")).get("password")
    except Exception:
        return None


def main():
    prompt = sys.argv[1] if len(sys.argv) > 1 else "Password: "
    answer = None
    if "password" in prompt.lower() and "yes/no" not in prompt:
        answer = from_vault()
        if answer is not None:
            print("[PixelVault] palavra-passe preenchida a partir do cofre.", file=sys.stderr)
    if answer is None:
        answer = ask_terminal(prompt)
    sys.stdout.write(answer + "\n")


if __name__ == "__main__":
    main()
