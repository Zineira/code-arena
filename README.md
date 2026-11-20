# PixelVault

Cofre local de palavras-passe e de acessos SSH, com um menu ao estilo dos jogos de blocos.

| Botão | O que faz |
|---|---|
| **Um Jogador** | As tuas palavras-passe: procurar, copiar, mostrar, gerar, editar. |
| **Multijogador** | Os teus servidores (endpoints). Cada um mostra o estado (barras de sinal, ms e a versão do SSH). **Entrar no Servidor** abre o terminal já ligado por `ssh`. |
| **Ligação Direta** | Liga a um `utilizador@anfitrião:porta` sem guardar nada. |
| **Opções...** | Bloqueio automático, limpeza da área de transferência, terminal, sons, mudar a palavra-passe mestra. |

## Instalar e correr

Precisas de Python 3.9+ e do cliente OpenSSH (já vem com o Windows 10/11, o macOS e quase todas as distribuições Linux).

```bash
cd pixelvault
pip install -r requirements.txt
python pixelvault.py
```

O browser abre sozinho em `http://127.0.0.1:<porta>/#t=<token>`. Na primeira vez escolhes a **palavra-passe mestra**. Para sair, usa **Sair** no menu ou `Ctrl+C` na consola.

Opções: `--port 47000` para fixar a porta, `--no-browser` para não abrir o browser. A variável `PIXELVAULT_HOME` muda a pasta do cofre (por omissão `~/.pixelvault`).

## Segurança: o que fica onde

- **Ficheiro do cofre:** `~/.pixelvault/vault.pv`, cifrado com **AES-256-GCM**. A chave é derivada da palavra-passe mestra com **scrypt** (N=32768, r=8, p=1). Nomes, endereços, utilizadores e palavras-passe ficam todos cifrados. No Linux e no macOS o ficheiro tem permissões `600`.
- **A palavra-passe mestra** não é guardada em lado nenhum. Se a perderes, o cofre não pode ser recuperado.
- **Servidor local:** só escuta em `127.0.0.1`. Cada execução gera um token aleatório, e todos os pedidos à API têm de o trazer. Pedidos com outro `Host` são recusados, o que impede ataques de DNS rebinding a partir de outros sites.
- **Palavras-passe dos servidores** nunca voltam ao browser depois de guardadas. Quando entras num servidor:
  1. o PixelVault cria um **ticket de uso único** (válido 2 minutos);
  2. abre o terminal com `ssh`, que tem `SSH_ASKPASS` a apontar para `askpass.py`;
  3. quando o ssh pede a palavra-passe, o `askpass.py` troca o ticket pela palavra-passe e entrega-a diretamente ao ssh.

  A palavra-passe não aparece na linha de comandos, nem no histórico, nem em ficheiros temporários. Qualquer outra pergunta (impressão digital do servidor na primeira ligação, 2FA, uma segunda tentativa) é feita a ti no terminal.
- **Bloqueio automático:** ao fim de 10 minutos sem atividade (configurável), a chave e os dados são apagados da memória.
- **Área de transferência:** ao copiar, o PixelVault limpa-a passado 30 s (configurável), desde que a janela ainda esteja aberta.
- **Tentativas erradas:** a partir da terceira tentativa errada da mestra há um tempo de espera crescente.

A melhor opção continua a ser a **autenticação por chave SSH**. Em *Autenticação: Chave SSH* indicas o ficheiro (por exemplo `~/.ssh/id_ed25519`) e o PixelVault guarda só o caminho.

## Terminais

- **Windows:** abre uma nova janela da consola com `ssh.exe` (OpenSSH do Windows). O preenchimento automático precisa do OpenSSH 8.4 ou mais recente. Se a tua versão não o suportar, o ssh pede a palavra-passe na própria janela.
- **macOS:** Terminal ou iTerm (escolhe nas Opções).
- **Linux:** deteta `x-terminal-emulator`, GNOME Terminal, Konsole, Xfce, Tilix, kitty, Alacritty, WezTerm, foot ou xterm. Escolhe nas Opções se tiveres vários.

## Ficheiros

```
pixelvault.py         servidor local, cofre cifrado e lançamento do ssh
askpass.py            programa SSH_ASKPASS que entrega a palavra-passe ao ssh
web/index.html        a interface (sem dependências externas, nada é carregado da internet)
web/pixel.ttf         fonte pixel original (gerada por tools/build_font.py)
test_pixelvault.py    testes: python test_pixelvault.py
```

A interface é original: as texturas, a paisagem do fundo, os ícones e a fonte são todos gerados pelo código. Não usa nenhum asset nem nenhuma marca de jogos existentes.
