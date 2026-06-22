# jq Cheatsheet

Processador de JSON na linha de comando. Combina com `curl`, `docker inspect` e qualquer API.

## Básico
```bash
echo '{"a":1}' | jq '.'          # pretty-print + valida
jq '.' file.json
curl -s api.com | jq '.'
jq -c '.' file.json              # compacto (uma linha)
jq -r '.name' file.json          # raw (string sem aspas)
```

## Acessar campos
```bash
jq '.name'                       # campo
jq '.user.email'                 # aninhado
jq '.items[0]'                   # índice do array
jq '.items[-1]'                  # último
jq '.items[]'                    # todos os elementos (itera)
jq '.["chave-com-traço"]'        # chave com caractere especial
jq '.field // "default"'         # fallback se null/ausente
jq '.a?'                         # não erra se .a não existir
```

## Slices e ranges
```bash
jq '.items[2:4]'                 # elementos 2 e 3
jq '.items[:3]'                  # primeiros 3
jq '.items[2:]'                  # do 3º em diante
```

## Pipes e seletores
```bash
jq '.items[] | .name'            # nome de cada item
jq '.users[] | {name, email}'    # projeta campos
jq '.users[] | .name, .age'
jq '.[] | select(.age > 18)'     # filtra
jq '.[] | select(.active == true)'
jq '.[] | select(.name | startswith("A"))'
jq '.users[] | select(.role == "admin") | .email'
```

## Transformar
```bash
jq 'map(.price)'                 # extrai campo de cada item
jq 'map(.price * 1.1)'           # transforma cada
jq 'map(select(.active))'        # filtra mantendo array
jq '[.users[].name]'             # coleta em novo array
jq '.items | length'             # tamanho
jq 'keys'                        # chaves do objeto
jq 'to_entries'                  # vira [{key,value}, ...]
jq 'sort_by(.age)'
jq 'group_by(.dept)'
jq 'unique'
jq 'reverse'
jq 'add'                         # soma/concatena array
```

## Construir objetos e arrays
```bash
jq '{nome: .name, idade: .age}'
jq '{name, email}'               # atalho (mesmo nome)
jq '.users | map({id, name})'
jq '{total: (.items | length), nomes: [.items[].name]}'
```

## Agregações
```bash
jq '[.items[].price] | add'              # soma
jq '[.items[].price] | add / length'     # média
jq '.items | map(.price) | max'
jq '.items | min_by(.age)'
jq '.items | max_by(.score)'
jq 'group_by(.dept) | map({dept: .[0].dept, n: length})'
```

## Strings e números
```bash
jq '.name | ascii_upcase'
jq '.name | ascii_downcase'
jq '.text | length'
jq '.csv | split(",")'
jq '.parts | join("-")'
jq '.text | ltrimstr("prefix")'
jq '"\(.name): \(.age)"'         # interpolação
jq '.n | tostring'
jq '.s | tonumber'
```

## Condicionais
```bash
jq 'if .age >= 18 then "adulto" else "menor" end'
jq '.[] | select(.tags | contains(["urgent"]))'
jq '.[] | select(.name | test("^A"; "i"))'   # regex (i = ignore case)
jq 'has("email")'                # tem a chave?
```

## Flags úteis
```bash
jq -r       # raw output (sem aspas em strings)
jq -c       # compacto
jq -s       # slurp (junta múltiplos JSONs num array)
jq -n       # null input (gerar JSON do zero)
jq -e       # exit code conforme resultado (pra scripts)
jq --arg nome "alice" '.users[] | select(.name == $nome)'
jq --argjson n 5 '.items[:$n]'
```

## Receitas práticas
```bash
# extrai IPs de docker inspect
docker inspect web | jq -r '.[0].NetworkSettings.IPAddress'

# converte JSON em linhas TSV
jq -r '.users[] | [.id, .name, .email] | @tsv'

# converte em CSV com header
jq -r '(.[0] | keys), (.[] | [.[]]) | @csv' data.json

# pega só os nomes de uma API paginada
curl -s api.com/users | jq -r '.data[].name'

# conta por categoria
jq -r 'group_by(.type)[] | "\(.[0].type): \(length)"' file.json

# filtra e reformata em uma linha
curl -s api.com | jq '[.[] | select(.active) | {id, name}]'
```

## Formatos de saída
```bash
@text   @json   @csv   @tsv   @base64   @uri   @sh
jq -r '.list | @csv'
jq -r '.cmd | @sh'        # escapa para shell
```
