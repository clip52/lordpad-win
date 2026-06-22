# Linux CLI Cheatsheet

Ferramentas de linha de comando e processamento de texto. Complementa o `bash.md` (sintaxe da shell).

## grep
```bash
grep "foo" file.txt
grep -i "foo"            # case-insensitive
grep -r "foo" dir/       # recursivo
grep -n "foo" file       # número da linha
grep -v "foo" file       # inverte (linhas SEM foo)
grep -c "foo" file       # conta ocorrências
grep -l "foo" *.txt      # só nomes dos arquivos que casam
grep -w "foo"            # palavra inteira
grep -A 3 "foo"          # 3 linhas depois (After)
grep -B 3 "foo"          # 3 antes (Before)
grep -C 3 "foo"          # 3 em volta (Context)
grep -E "foo|bar"        # regex estendida (ERE)
grep -P "\d{3}"          # regex Perl (PCRE)
grep -o "[0-9]\+"        # só a parte que casou
```

## find
```bash
find . -name "*.py"               # por nome (glob)
find . -iname "*.PY"              # case-insensitive
find . -type f                    # arquivos
find . -type d                    # diretórios
find . -mtime -7                  # modificados nos últimos 7 dias
find . -size +10M                 # maiores que 10MB
find . -empty                     # vazios
find . -maxdepth 2                # limita profundidade
find . -name "*.log" -delete      # acha e deleta
find . -name "*.txt" -exec grep "foo" {} +   # roda comando
find . -name "*.tmp" -exec rm {} \;
```

## sed (stream editor)
```bash
sed 's/old/new/' file        # substitui 1ª ocorrência por linha
sed 's/old/new/g' file       # todas
sed 's/old/new/gi' file      # global + case-insensitive
sed -i 's/old/new/g' file    # edita o arquivo in-place
sed -i.bak 's/old/new/g' f   # in-place com backup .bak
sed -n '5,10p' file          # imprime só linhas 5 a 10
sed '3d' file                # deleta linha 3
sed '/foo/d' file            # deleta linhas com foo
sed 's/[0-9]//g' file        # remove dígitos
sed -E 's/(\w+) (\w+)/\2 \1/' # grupos e backreference (ERE)
```

## awk
```bash
awk '{print $1}' file              # 1ª coluna
awk '{print $NF}' file             # última coluna
awk -F',' '{print $2}' file        # separador vírgula
awk '{print $1, $3}' file
awk 'NR==1' file                   # primeira linha
awk 'NR>1' file                    # pula header
awk '$3 > 100' file                # filtra por valor
awk '/foo/ {print $2}' file        # casa padrão e imprime
awk '{sum += $1} END {print sum}'  # soma coluna
awk '{print NR": "$0}'             # numera linhas
awk 'length > 80'                  # linhas longas
awk -F: '{print $1}' /etc/passwd   # usuários
```

## sort / uniq / cut / tr / wc
```bash
sort file                    # alfabético
sort -n file                 # numérico
sort -r file                 # reverso
sort -k2 file                # pela 2ª coluna
sort -u file                 # único + ordenado
sort file | uniq             # remove duplicatas adjacentes
sort file | uniq -c          # conta ocorrências
sort | uniq -c | sort -rn    # ranking (top hits)

cut -d',' -f1,3 file         # campos 1 e 3, separador vírgula
cut -c1-10 file              # caracteres 1 a 10

tr 'a-z' 'A-Z' < file        # uppercase
tr -d '0-9' < file           # deleta dígitos
tr -s ' ' < file             # comprime espaços repetidos
tr '\n' ' ' < file           # troca newline por espaço

wc -l file                   # conta linhas
wc -w file                   # palavras
wc -c file                   # bytes
```

## head / tail
```bash
head file               # 10 primeiras linhas
head -n 20 file
tail file               # 10 últimas
tail -n 50 file
tail -f log.txt         # acompanha em tempo real
tail -f log | grep ERROR
```

## xargs
```bash
ls *.txt | xargs rm
find . -name "*.log" | xargs grep "error"
echo "a b c" | xargs -n1          # um por linha
find . -name "*.jpg" | xargs -P4 -I{} convert {} {}.png  # 4 em paralelo
cat urls.txt | xargs -I{} curl -O {}
```

## Pipes encadeados (exemplos reais)
```bash
# top 10 IPs num access.log
awk '{print $1}' access.log | sort | uniq -c | sort -rn | head

# tamanho total de arquivos .log
find . -name "*.log" | xargs du -ch | tail -1

# processos consumindo mais memória
ps aux | sort -k4 -rn | head
```

## Arquivos compactados
```bash
tar -czf arch.tar.gz dir/      # cria gzip
tar -xzf arch.tar.gz           # extrai gzip
tar -tzf arch.tar.gz           # lista conteúdo
tar -xzf arch.tar.gz -C /dest  # extrai em destino
zip -r arch.zip dir/
unzip arch.zip
gzip file        # vira file.gz
gunzip file.gz
```

## curl / wget
```bash
curl https://api.com/data
curl -O https://site.com/file.zip       # salva com nome original
curl -o out.json https://api.com         # nome custom
curl -L url                              # segue redirects
curl -I url                              # só headers (HEAD)
curl -X POST -H "Content-Type: application/json" \
     -d '{"x":1}' https://api.com
curl -H "Authorization: Bearer $TOKEN" url
curl -s url | jq                         # silencioso + jq
curl -w "%{http_code}" -o /dev/null -s url   # só o status

wget url
wget -r -np url                          # recursivo
wget -c url                              # continua download interrompido
```

## ssh / scp / rsync
```bash
ssh user@host
ssh -p 2222 user@host
ssh -i ~/.ssh/key user@host
ssh user@host 'comando remoto'

scp file.txt user@host:/path/
scp user@host:/path/file.txt .
scp -r dir/ user@host:/path/

rsync -avz src/ user@host:/dest/    # sync incremental, comprimido
rsync -avz --delete src/ dest/      # espelha (apaga extras no destino)
rsync -avzn src/ dest/              # dry-run (-n simula)
```

## Processos
```bash
ps aux                  # todos os processos
ps aux | grep nginx
top                     # monitor ao vivo
htop                    # versão melhor (se instalado)
kill PID
kill -9 PID             # SIGKILL (forçado)
killall nginx
pkill -f "python app"   # por padrão no comando
pgrep -f python
nice -n 10 cmd          # roda com prioridade baixa
```

## Disco / sistema
```bash
df -h                   # espaço por partição (human)
du -sh dir/             # tamanho total da pasta
du -sh * | sort -rh     # maiores itens do diretório
free -h                 # memória
lsof -i :8080           # quem usa a porta 8080
lsof file.txt           # quem abriu o arquivo
uptime
```

## Permissões
```bash
chmod +x script.sh
chmod 644 file          # rw-r--r--
chmod 755 dir           # rwxr-xr-x
chmod -R 755 dir/
chown user:group file
chown -R user dir/
ln -s /target link      # symlink
ln target hardlink
umask 022
```
