# Iriseus — Contexto e Visão do Projeto

Iris + Caduceus

## O que foi feito até agora (WebcamStream)

### Problema original
Necessidade de usar o celular como webcam em entrevistas técnicas em empresas grandes,
que exigem câmera ligada durante provas online. O PC não possui câmera integrada.

### Solução implementada (gambiarra funcional)

O app atual resolve o problema, mas com diversas dependências externas e configuração
manual pesada.

**Stack:**
- App Flutter (Android) com pacote `rtmp_broadcaster` — captura câmera e transmite via RTMP
- `nginx-rtmp` no PC — servidor RTMP que recebe o stream do celular
- OBS Studio — consome o stream do nginx via Fonte de Mídia e expõe como Virtual Camera
- Script Python (`setup_obs/setup.py`) — automatiza `adb reverse` para modo USB

**Fluxo:**

```
[Flutter Android]
↓ RTMP (WiFi ou USB via ADB reverse)
[nginx :1935]
↓ rtmp://localhost/live
[OBS — Fonte de Mídia]
↓
[OBS Virtual Camera]
↓
[Meet / Zoom / Teams — enxerga como webcam]
```


**Modos de conexão:**
- WiFi: celular transmite para o IP do PC na porta 1935
- USB: script Python roda `adb reverse tcp:1935 tcp:1935`, celular transmite para
  `127.0.0.1:1935`

**Por que é uma gambiarra:**
- Depende de OBS instalado (software pesado, feito para streaming/gravação)
- Depende de nginx-rtmp instalado e configurado manualmente
- OBS Fonte de Mídia é cliente, não servidor — não recebe RTMP diretamente, por isso o
  nginx é necessário como intermediário
- IP do PC digitado manualmente no app — sem descoberta automática
- SDK 37 do Android exigido pelo RootEncoder (dependência do `rtmp_broadcaster`), mas o
  SDK Manager instala com ID `android-37.0` em vez de `android-37` — necessário criar
  symlink manualmente
- Qualquer pessoa que quiser usar precisa instalar e configurar OBS + nginx + Python + ADB
  antes de abrir o app
- Microfone não configurável — vem do celular por padrão, sem opção de manter o
  microfone do PC

**O que funciona:**
- Celular aparece como webcam no Meet, Zoom e Teams via OBS Virtual Camera
- Modo USB estável via ADB reverse
- Modo WiFi funcional na mesma rede

---

## O que quero construir

### Visão geral

Plataforma de comunicação local entre dispositivos — celular ou computador — via WiFi ou
USB, sem dependência de serviços externos, sem configuração manual, sem software
intermediário.

O foco inicial é suporte a Android e Windows, mas todas as decisões de arquitetura devem
ser tomadas pensando em multiplataforma real: qualquer celular (Android, iOS futuro),
qualquer computador (Windows, macOS, Linux futuros). Nada que amarre a uma plataforma
específica sem necessidade.

iOS está fora do escopo por ora — o app Flutter seria reaproveitável, mas o companion
no macOS exigiria DAL plugin nativo + Apple Developer account + notarização, o que
representa um target separado independentemente do Flutter.

### Problema que resolve

Soluções existentes são ruins por diferentes razões:

| Solução | Problema |
|---|---|
| OBS + nginx (atual) | Pesado, configuração manual, não é para isso |
| Google Drive / AirDrop | Exige internet, não é tempo real |
| Samsung Flow / Intel Unison | Só funciona com hardware específico |
| KDE Connect | Mais próximo do ideal, mas sem webcam virtual e UI ruim no Windows |

### Fase 1 — Câmera como webcam virtual (sem intermediários)

A primeira fase já resolve o problema original, mas de forma limpa:

**O que o usuário faz:**
1. Instala o app no celular
2. Instala o app no PC (que registra o driver de webcam virtual automaticamente)
3. Conecta os dois via QR Code ou PIN — pareamento único
4. A partir daí, sempre que estiverem na mesma rede ou conectados via USB, o celular
   pode ativar o compartilhamento da câmera

**O que acontece por baixo:**

```
[Flutter Android]
↓ H264 via socket TCP (WiFi / USB via ADB reverse)
[App Windows — recebe e decodifica stream]
↓
[Softcam — DirectShow filter registrado no sistema]
↓ scSendFrame()
[Meet / Zoom / Teams — enxerga como webcam nativa]
```


Sem OBS. Sem nginx. Sem configuração manual.

**Microfone:**
- Por padrão, o microfone do PC é mantido como entrada de áudio — o usuário não perde
  o microfone que já usa
- O app oferece a opção de alternar para o microfone do celular se desejado
- As duas fontes são independentes e configuráveis separadamente

### Fase 2 — Transferência de arquivos

- Transferência de arquivos em qualquer direção (celular → PC, PC → celular,
  celular → celular, PC → PC)
- Fotos, vídeos, documentos — qualquer formato
- Progresso em tempo real
- Sem limite de tamanho (transferência local, sem passar por servidor)

### Fase 3 — Expansão

- Área de transferência sincronizada (copiar no celular, colar no PC e vice-versa)
- Notificações do celular espelhadas no PC
- Microfone do celular como entrada de áudio virtual no PC (independente da câmera)
- Compartilhamento de tela (celular → PC, PC → celular, celular → celular, PC → PC)
- Controle remoto básico

---

## Arquitetura técnica

### Princípio geral

Todas as decisões devem funcionar ou ter caminho claro para funcionar em qualquer
plataforma. Nada proprietário sem alternativa. Protocolos abertos sempre que possível.

### Comunicação

```
Descoberta: mDNS (multicast DNS) — padrão aberto, funciona em qualquer OS
Controle: WebSocket (JSON) — bidirecional, leve, universal
Arquivos: HTTP chunked ou gRPC streaming
Câmera/áudio: H264 via socket TCP direto
Segurança: TLS + ECDH no pareamento (TOFU)
USB: ADB reverse (Android) — detectado automaticamente pelo app Windows
```


### Por que não WebRTC para o streaming

WebRTC seria o ideal em cenários peer-to-peer com NAT traversal, múltiplos peers ou
clientes browser. No caso do DevLink, os dispositivos estão sempre na mesma rede local
ou conectados via USB — NAT traversal, STUN/TURN e signaling server são overhead
desnecessário.

A implementação de WebRTC em C++ nativo exige libwebrtc (build de ~1GB, dependências
do Chromium build system, tempo de build de horas). Para Fase 1, o custo não se
justifica.

**Stack de streaming adotado:**

```
Android: CameraX → MediaCodec (H264 hardware-encoded) → socket TCP
Windows: socket TCP → FFmpeg (decodifica H264) → Softcam → scSendFrame()
```


FFmpeg no Windows é facilmente bundlável (LGPL, binários pré-compilados disponíveis).
Latência comparável ao WebRTC em rede local. Se no futuro o DevLink precisar de relay
por internet ou suporte a browser, WebRTC pode ser introduzido sem quebrar a arquitetura
— o canal de controle WebSocket já seria o signaling.

### Stack

| Componente | Tecnologia | Justificativa |
|---|---|---|
| App mobile | Flutter | Multiplataforma — Android hoje, iOS no futuro sem reescrita |
| App desktop | C++ nativo | Softcam/DirectShow e FFmpeg exigem acesso nativo |
| Futuramente macOS | Swift + DAL plugin | Driver de câmera virtual no macOS exige extensão nativa |
| Futuramente Linux | C + V4L2 | Módulo de câmera virtual via v4l2loopback |
| Descoberta de rede | mDNS — `nsd` no Android, `dns-sd` / `avahi` no desktop | Padrão aberto, sem servidor central |
| Protocolo de controle | WebSocket | Funciona em qualquer linguagem e plataforma |
| Streaming de câmera | H264 via socket TCP | Leve, sem overhead de WebRTC, decodifica via FFmpeg |
| Driver webcam Windows | Softcam (DirectShow filter) | MIT, userspace DLL, Win7+, sem kernel driver |
| Decodificação no PC | FFmpeg | LGPL, binários pré-compilados, fácil de bundlar |
| USB | ADB reverse + socket local | Único caminho viável para Android sem root |

### Driver de webcam virtual — Softcam

O Windows tem duas APIs para câmeras virtuais:

- **Media Foundation Virtual Camera** (`IMFVirtualCamera`) — API oficial Microsoft,
  sem necessidade de kernel driver, mas exige Win10 build 19041+ (20H1). Incompatível
  com o requisito mínimo de Win10.
- **DirectShow filter** — funciona desde Win7, userspace DLL registrada via `regsvr32`,
  sem necessidade de kernel driver assinado (`.sys`). É o caminho correto para
  compatibilidade ampla.

**Softcam** ([github.com/tshino/softcam](https://github.com/tshino/softcam)) implementa
um DirectShow filter de categoria Video Input Device. Uma vez registrado, qualquer
aplicativo que use DirectShow API reconhece como webcam. A API é mínima:

```cpp
scCamera cam = scCreateCamera(1280, 720, 30);
// loop de recebimento de frames do socket...
scSendFrame(cam, frameBuffer);
```

Licença MIT. Compatível com Zoom, Teams, Meet, OBS, Discord. O installer do DevLink
registra a DLL silenciosamente via `regsvr32` — transparente para o usuário.

### Por que o app Windows não pode ser Flutter

Softcam exige registro de componente COM e integração com DirectShow — camada nativa
do Windows. FFmpeg para decodificação também é nativo. Flutter não tem acesso a essa
camada. O app Windows precisa ser C++ nativo para:

- Registrar e chamar Softcam via DirectShow
- Decodificar H264 com FFmpeg
- Gerenciar o socket de recebimento do stream

A UI do app Windows pode ser simples — ícone na bandeja do sistema (systray) com janela
de status e configurações.

### Descoberta automática de dispositivos

```
App Windows sobe serviço mDNS anunciando "_devlink._tcp.local"
App Android faz scan mDNS e lista dispositivos encontrados
Usuário seleciona o PC na lista — sem digitar IP
Pareamento via QR Code ou PIN de 6 dígitos — único, salvo para reconexões futuras
Próximas conexões: automáticas, sem interação
```

### Modo USB — ADB bundlado

O app Windows não depende do ADB instalado no sistema do usuário. Os três arquivos
necessários são bundlados no installer:

```
adb.exe
AdbWinApi.dll
AdbWinUsbApi.dll
```


Licença Apache 2.0 — redistribuição permitida. Extraídos para
`%LOCALAPPDATA%\DevLink\adb\` na instalação. O app sempre chama `adb.exe` por path
absoluto, nunca pelo PATH do sistema.

Para evitar conflito com um servidor ADB já em execução no sistema do usuário, o DevLink
usa uma porta customizada:

```cpp
SetEnvironmentVariable(L"ANDROID_ADB_SERVER_PORT", L"5038");
// chama adb.exe reverse, start-server etc. via path absoluto
```

Isso isola completamente o servidor ADB do DevLink do servidor ADB do usuário.

**Fluxo de detecção USB:**

```
App Windows monitora conexão de dispositivos via ADB em background
Celular conectado via USB → app detecta automaticamente
Roda adb reverse para redirecionar as portas necessárias
App mobile detecta que está em modo USB e usa 127.0.0.1
Tudo transparente para o usuário
```

### Segurança — pareamento e canal

**Modelo adotado: TOFU (Trust On First Use)**, o mesmo usado por SSH e KDE Connect.

**Fluxo de pareamento:**

```
App Windows gera par de chaves ECDH efêmero
QR Code contém: endereço IP + porta + chave pública do PC
App Android escaneia QR, envia sua própria chave pública
Ambos derivam segredo compartilhado via ECDH
A partir daí, canal protegido por TLS com a chave derivada
Identidades salvas — reconexões futuras são automáticas e autenticadas
```


**Ataque hipotético ao QR:**

Em redes compartilhadas, um atacante poderia subir um serviço mDNS falso anunciando
`_devlink._tcp.local` antes do PC legítimo, fazer o app Android listar o dispositivo
falso, e induzir o usuário a escanear um QR malicioso — pareando com o atacante.

Na prática, o ataque exige: atacante na mesma rede local, e usuário escaneando QR
exibido em tela que não é a do seu PC. O QR é exibido na tela do PC — se o usuário
vê a tela do seu PC, não há vetor.

A mitigação suficiente é UI clara: exibir nome e ID do dispositivo nos dois lados
durante o pareamento, para confirmação visual explícita. Nenhum mecanismo adicional
é necessário para o caso de uso local.

**Pós-pareamento:**

Atacante que não participou do pareamento não consegue descriptografar o canal —
o segredo ECDH nunca trafega na rede. A segurança pós-pareamento não depende da
segurança da rede.

---

## Diferenças fundamentais em relação ao WebcamStream atual

| | WebcamStream (atual) | DevLink (objetivo) |
|---|---|---|
| Dependências externas | OBS + nginx + Python + ADB | Nenhuma visível ao usuário |
| Configuração | Manual (IP, nginx.conf, OBS) | Zero — QR Code uma vez |
| Driver webcam | OBS Virtual Camera | Softcam (DirectShow, MIT) |
| Streaming | RTMP via nginx | H264 via socket TCP direto |
| Microfone | Celular por padrão, sem escolha | PC por padrão, configurável |
| Descoberta de dispositivos | IP manual | mDNS automático |
| ADB | Instalado pelo usuário | Bundlado, isolado, transparente |
| Multiplataforma | Android + Windows only | Arquitetura preparada para qualquer OS |
| Transferência de arquivos | Não tem | Fase 2 |
| Instalação para o usuário final | Complexa | Instala app no celular + app no PC, pronto |

---

## Ordem de desenvolvimento

1. **Protocolo base** — descoberta mDNS + WebSocket de controle + pareamento QR/PIN
2. **Streaming de câmera + Softcam** — H264 via socket, scSendFrame(), resolve o
   problema original sem gambiarra
3. **Modo USB** — ADB watcher automático, porta isolada, adb reverse transparente
4. **Transferência de arquivos** — valida o canal de dados
5. **Microfone virtual** — independente da câmera
6. **Área de transferência, notificações** — incrementais
7. **Compartilhamento de tela** — maior complexidade, última fase

