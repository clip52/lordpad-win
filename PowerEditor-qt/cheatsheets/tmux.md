# tmux Cheatsheet

Prefixo padrão: `Ctrl+b`. Notação `<prefix> X` = aperte Ctrl+b, solte, depois X.

## Por que tmux
Mantém sessões vivas mesmo depois de desconectar do SSH, divide o terminal em painéis e permite retomar exatamente de onde parou. Combina bem com vim e trabalho remoto.

## Sessões (pela shell)
```bash
tmux                      # nova sessão
tmux new -s trabalho      # nova sessão nomeada
tmux ls                   # lista sessões
tmux attach               # reconecta à última
tmux attach -t trabalho   # reconecta a uma específica
tmux kill-session -t trabalho
tmux kill-server          # mata tudo
```

## Sessões (dentro do tmux)
```
<prefix> d        desanexa (detach) - sessão continua viva
<prefix> s        lista/troca de sessão
<prefix> $        renomeia sessão
<prefix> (        sessão anterior
<prefix> )        próxima sessão
```

## Janelas (windows = abas)
```
<prefix> c        nova janela
<prefix> ,        renomeia janela
<prefix> n        próxima janela
<prefix> p        janela anterior
<prefix> 0..9     vai para janela N
<prefix> w        lista janelas
<prefix> &        fecha janela (confirma)
<prefix> f        busca janela por nome
```

## Painéis (panes = divisões)
```
<prefix> %        split vertical (lado a lado)
<prefix> "        split horizontal (em cima/embaixo)
<prefix> →↑↓←     navega entre painéis (setas)
<prefix> o        próximo painel
<prefix> ;        último painel ativo
<prefix> x        fecha painel (confirma)
<prefix> z        zoom no painel (toggle tela cheia)
<prefix> {        move painel para esquerda
<prefix> }        move painel para direita
<prefix> espaço   cicla layouts predefinidos
<prefix> q        mostra números dos painéis
<prefix> Ctrl+→↑↓← redimensiona painel
```

## Copy mode (scroll e seleção)
```
<prefix> [        entra em copy mode
  ↑↓ PgUp PgDn    navega/scroll
  /  ?            busca (frente/trás)
  espaço          inicia seleção
  enter           copia e sai
q                 sai do copy mode
<prefix> ]        cola
```
Com `mode-keys vi` no config, a seleção usa `v` para iniciar e `y` para copiar (igual vim).

## Comandos (linha de comando do tmux)
```
<prefix> :        abre prompt de comando
:new-window -n logs
:rename-session prod
:kill-pane
:resize-pane -D 10        (D/U/L/R = direção, 10 = células)
:source-file ~/.tmux.conf  recarrega config
```

## ~/.tmux.conf (config recomendada)
```bash
# troca o prefixo para Ctrl+a (mais ergonômico)
unbind C-b
set -g prefix C-a
bind C-a send-prefix

# splits mais intuitivos
bind | split-window -h
bind - split-window -v

# navegação estilo vim entre painéis
bind h select-pane -L
bind j select-pane -D
bind k select-pane -U
bind l select-pane -R

# começa contagem em 1, não 0
set -g base-index 1
setw -g pane-base-index 1

# mouse (scroll, seleção, redimensionar)
set -g mouse on

# copy mode estilo vim
setw -g mode-keys vi

# histórico maior
set -g history-limit 10000

# reload rápido do config
bind r source-file ~/.tmux.conf \; display "Recarregado!"

# evita delay do ESC (importante pra vim)
set -sg escape-time 0
```

## Fluxo típico
```bash
# inicia ou reconecta a uma sessão de projeto
tmux new -s app 2>/dev/null || tmux attach -t app

# no SSH: roda algo longo, desanexa, fecha o laptop, volta depois
ssh server
tmux new -s build
npm run build:long
# <prefix> d para desanexar, encerra o SSH, o build continua
# mais tarde: ssh server && tmux attach -t build
```
