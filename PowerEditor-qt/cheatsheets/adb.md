# ADB Cheatsheet

Android Debug Bridge: ponte de comunicação com dispositivos e emuladores Android via USB ou rede.

## Pré-requisito
No aparelho: ative **Opções do desenvolvedor** (toque 7x em "Número da versão" nas Configurações) e ligue a **Depuração USB**. Na primeira conexão, aceite o diálogo de autorização do computador.

## Conexão e dispositivos
```bash
adb devices                  # lista dispositivos conectados
adb devices -l               # com modelo/detalhes
adb get-state                # device | offline | unknown
adb -s SERIAL <cmd>          # mira um aparelho específico (multi-device)
adb -d <cmd>                 # único dispositivo USB
adb -e <cmd>                 # único emulador

adb start-server
adb kill-server              # resolve a maioria dos "device offline"
adb reconnect                # força reconexão
```

## ADB via Wi-Fi
```bash
# método clássico (precisa de USB uma vez)
adb tcpip 5555               # coloca o aparelho em modo TCP/IP
adb shell ip route           # descobre o IP do aparelho
adb connect 192.168.0.42:5555
adb disconnect 192.168.0.42:5555

# Android 11+ : pareamento sem fio (sem USB)
# pegue IP:porta e código no menu "Depuração sem fio"
adb pair 192.168.0.42:37000  # digita o código de 6 dígitos
adb connect 192.168.0.42:35000
```

## Apps (install / uninstall)
```bash
adb install app.apk
adb install -r app.apk           # reinstala mantendo dados
adb install -d app.apk           # permite downgrade
adb install -g app.apk           # concede todas as permissões
adb install-multiple *.apk       # split APKs (app bundles)

adb uninstall com.exemplo.app
adb uninstall -k com.exemplo.app # remove app, mantém dados/cache

# extrair o APK de um app instalado
adb shell pm path com.exemplo.app
adb pull /data/app/.../base.apk ./app.apk
```

## Pacotes (pm)
```bash
adb shell pm list packages              # todos
adb shell pm list packages -3           # só de terceiros
adb shell pm list packages -s           # só do sistema
adb shell pm list packages | grep whats # busca
adb shell pm clear com.exemplo.app      # limpa dados (reset)
adb shell pm disable-user com.exemplo.app  # desativa (bloatware)
adb shell pm enable com.exemplo.app
adb shell pm grant com.app android.permission.CAMERA
adb shell pm revoke com.app android.permission.CAMERA
adb shell dumpsys package com.exemplo.app  # info detalhada
```

## Transferência de arquivos
```bash
adb push arquivo.txt /sdcard/         # PC → aparelho
adb push pasta/ /sdcard/destino/
adb pull /sdcard/foto.jpg ./          # aparelho → PC
adb pull /sdcard/DCIM/ ./backup/

# locais úteis
# /sdcard/            armazenamento interno do usuário
# /sdcard/DCIM/Camera fotos
# /sdcard/Download
```

## Shell
```bash
adb shell                    # abre shell interativo
adb shell <comando>          # roda um comando e sai
adb shell ls /sdcard
adb shell df -h              # espaço em disco
adb shell getprop            # todas as propriedades do sistema
adb shell getprop ro.product.model
adb shell getprop ro.build.version.release   # versão do Android
adb shell wm size            # resolução da tela
adb shell wm density         # densidade (dpi)
adb shell settings get system screen_brightness
adb shell settings put system screen_brightness 100
```

## Logcat
```bash
adb logcat                       # log ao vivo (tudo)
adb logcat -c                    # limpa o buffer
adb logcat *:E                   # só erros (E=Error)
adb logcat *:W                   # warnings pra cima
adb logcat | grep com.app
adb logcat --pid=$(adb shell pidof com.app)  # só do seu app
adb logcat -d > log.txt          # dump e sai (-d)
adb logcat -v time               # com timestamp
adb logcat ActivityManager:I *:S # filtra por tag (S=Silent no resto)
```
Níveis: `V` verbose, `D` debug, `I` info, `W` warning, `E` error, `F` fatal, `S` silent.

## Captura de tela e gravação
```bash
adb shell screencap /sdcard/tela.png
adb pull /sdcard/tela.png

# atalho de uma linha (sem deixar arquivo no aparelho)
adb exec-out screencap -p > tela.png

adb shell screenrecord /sdcard/video.mp4   # Ctrl+C pra parar
adb shell screenrecord --time-limit 30 --bit-rate 8000000 /sdcard/v.mp4
adb pull /sdcard/video.mp4
```

## Simular input (eventos)
```bash
adb shell input tap 500 1000             # toque em x,y
adb shell input swipe 300 1000 300 200   # swipe (x1 y1 x2 y2)
adb shell input swipe 300 1000 300 200 500  # com duração em ms
adb shell input text "ola%smundo"        # %s = espaço
adb shell input keyevent 4               # BACK
adb shell input keyevent 3               # HOME
adb shell input keyevent 26              # POWER (liga/desliga tela)
adb shell input keyevent 66              # ENTER
adb shell input keyevent 187             # alterna apps recentes
adb shell input keyevent KEYCODE_VOLUME_UP
```

## Activities e intents (am)
```bash
adb shell am start -n com.app/.MainActivity       # abre activity
adb shell am start -a android.intent.action.VIEW -d https://exemplo.com
adb shell am force-stop com.exemplo.app           # mata o app
adb shell am start -a android.intent.action.CALL -d tel:11999999999
adb shell monkey -p com.app -v 500                # 500 eventos aleatórios (stress test)
```

## Energia e tela
```bash
adb shell dumpsys battery                 # estado da bateria
adb shell dumpsys battery set level 50    # finge nível (teste)
adb shell dumpsys battery reset
adb shell input keyevent 26               # toggle tela
adb shell svc power stayon true           # mantém ligada com USB
```

## Reboot e modos
```bash
adb reboot                   # reinicia normal
adb reboot recovery          # modo recovery
adb reboot bootloader        # bootloader/fastboot
adb reboot fastboot
adb root                     # reinicia adbd como root (só em builds que permitem)
adb unroot
adb remount                  # remonta /system como gravável (precisa root)
```

## Backup
```bash
adb backup -apk -all -f backup.ab        # backup completo (deprecado no Android novo)
adb restore backup.ab
# em versões recentes, prefira pull de /sdcard ou ferramentas dedicadas
```

## scrcpy (espelhar e controlar a tela)
Ferramenta separada, mas o complemento mais usado do adb. Espelha e controla o aparelho pelo PC com baixa latência.
```bash
scrcpy                       # espelha (com adb já conectado)
scrcpy --record saida.mp4    # grava enquanto espelha
scrcpy --no-audio
scrcpy -m 1024               # limita resolução (mais leve)
scrcpy --turn-screen-off     # tela do aparelho apagada, controla pelo PC
```

## Diagnóstico de problemas comuns
```bash
adb devices                  # "unauthorized" → aceite o diálogo no aparelho
adb kill-server && adb start-server   # "device offline"
# nada aparece: confira cabo de DADOS (não só carga) e driver USB (Windows)
# Linux: pode faltar regra udev; rode adb como usuário com permissão USB
```
