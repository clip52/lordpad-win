# Bash Cheatsheet

## Variáveis
```bash
name="alice"
echo "$name"          # expansão
echo '${name}'        # literal (sem expandir)
unset name
readonly PI=3.14
local x=1             # dentro de função
declare -i n=42       # tipado integer
declare -A map=([a]=1 [b]=2)  # array associativo
```

## Strings
```bash
${var:-default}       # var ou default se vazio
${var:=default}       # idem + atribui
${var:offset:length}  # substring
${var//pad/rep}       # replace all
${#var}               # tamanho
${var^^}              # uppercase
${var,,}              # lowercase
```

## Arrays
```bash
arr=(a b c)
echo "${arr[0]}"      # primeiro
echo "${arr[@]}"      # todos
echo "${#arr[@]}"     # tamanho
arr+=(d)              # append
for x in "${arr[@]}"; do echo "$x"; done
```

## Condicionais
```bash
if [[ "$a" == "$b" ]]; then ...; fi
if [[ -f file ]]; then ...; fi      # existe arquivo
if [[ -d dir  ]]; then ...; fi      # existe dir
if [[ -z "$x" ]]; then ...; fi      # vazio
if [[ -n "$x" ]]; then ...; fi      # não-vazio
[[ "$a" =~ ^[0-9]+$ ]] && echo num  # regex
```

## Loops
```bash
for i in {1..10}; do echo "$i"; done
for ((i=0; i<10; i++)); do echo "$i"; done
while read -r line; do echo "$line"; done < file.txt
until [[ "$x" == "stop" ]]; do read x; done
```

## Funções
```bash
my_fn() {
    local arg="$1"
    echo "got $arg"
    return 0
}
my_fn "hello"
```

## Redireção
```bash
cmd > out.txt          # stdout → file (sobrescreve)
cmd >> out.txt         # append
cmd 2> err.txt         # stderr → file
cmd > all 2>&1         # stdout+stderr juntos
cmd &> all             # idem (bash)
cmd < input.txt        # stdin de file
cmd1 | cmd2            # pipe
cmd <(other_cmd)       # process substitution
```

## Job control
```bash
cmd &                  # background
cmd > /dev/null 2>&1 & # background sem mensagens de debug na tela
jobs                   # listar
fg %1                  # foreground job 1
bg %1                  # resume in bg
disown -h %1           # imune a SIGHUP
nohup cmd &            # imune a logout
trap 'echo INT' INT    # catch Ctrl+C
```

## Substituição
```bash
$(cmd)                 # output como string
`cmd`                  # antigo, evite
$((1+2))               # arithmetic
```

## Strict mode
```bash
set -euo pipefail      # error on undef, fail on pipe error
IFS=$'\n\t'
```

## Atalhos no shell
```
Ctrl+A   início linha
Ctrl+E   fim linha
Ctrl+R   busca histórico
Ctrl+W   apaga palavra anterior
Ctrl+U   apaga linha
Alt+B/F  pula palavra
!!       último comando
!$       último argumento
```
