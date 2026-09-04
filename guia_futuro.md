# Iriseus — Guia Completo do App Windows

Este documento descreve o estado atual do app Windows e serve como roteiro para
implementação das próximas fases em um novo contexto.

---

## Estado atual

### O que está implementado

- Systray com menu (iniciar/parar câmera, parear dispositivo, sair)
- Servidor WebSocket na porta `45678` — canal de controle bidirecional
- Descoberta de rede via mDNS (`_iriseus._tcp.local`)
- Pareamento via QR Code + PIN de 6 dígitos (ECDH X25519 + TOFU via libsodium)
- Modo USB via ADB bundlado, porta isolada `5038`, `adb reverse` automático
- Pipeline de câmera: socket TCP porta `45679` → FFmpeg H264 decode → RGB24 → Softcam
- Webcam virtual via Softcam (DirectShow filter, MIT) carregado via LoadLibrary em runtime

### O que está pendente

- Transferência de arquivos (Fase 2)
- UI completa — janela abrindo ao duplo clique no ícone do systray (Fase UI)
- Microfone virtual (Fase 3)
- Área de transferência sincronizada (Fase 3)
- Notificações espelhadas (Fase 3)
- Compartilhamento de tela (Fase 3)
- Installer NSIS (após UI polida)

---

## Arquitetura técnica

### Stack

| Componente | Tecnologia |
|---|---|
| UI | Qt 6.11 (MSVC 2022 64-bit) |
| Build | CMake 3.24+ |
| WebSocket | Boost.Beast + Boost.Asio |
| JSON | nlohmann/json 3.11.3 |
| mDNS | mjansson/mdns |
| QR Code | nayuki/QR-Code-generator |
| Criptografia | libsodium 1.0.22 (estático, SODIUM_STATIC) |
| H264 decode | FFmpeg BtbN shared (avcodec, avutil, swscale) |
| Webcam virtual | Softcam (DirectShow, MIT) via LoadLibrary runtime |
| USB | ADB bundlado em %LOCALAPPDATA%\Iriseus\adb\ porta 5038 |

### Portas

| Porta | Uso |
|---|---|
| `45678` | WebSocket de controle |
| `45679` | Stream H264 TCP (câmera) |
| `5353` | mDNS multicast |
| `5038` | Servidor ADB interno isolado |

### Estrutura de arquivos

```
src/
├── main.cpp
├── app/
│ ├── Application.h / .cpp ← orquestrador principal
├── ui/
│ ├── TrayIcon.h / .cpp ← systray + menu
│ └── PairingDialog.h / .cpp ← janela QR Code + PIN
└── core/
├── WsServer.h / .cpp ← servidor WebSocket
├── WsSession.h / .cpp ← sessão individual por dispositivo
├── MessageDispatcher.h ← enum MsgType + parseMessage()
├── MdnsService.h / .cpp ← anúncio mDNS
├── PairingManager.h / .cpp ← TOFU + ECDH + persistência
├── PairingCrypto.h / .cpp ← X25519 via libsodium
├── AdbManager.h / .cpp ← ADB bundlado + adb reverse automático
├── StreamReceiver.h / .cpp ← socket TCP + framing
├── VideoDecoder.h / .cpp ← FFmpeg H264 → RGB24
└── VirtualCamera.h / .cpp ← Softcam via LoadLibrary
third_party/
└── softcam/ ← git submodule tshino/softcam
resources/
├── adb/
│ ├── adb.exe
│ ├── AdbWinApi.dll
│ └── AdbWinUsbApi.dll
└── resources.qrc

---

## Dependências para build

### 1. Visual Studio 2022 ou 2026
- https://visualstudio.microsoft.com/downloads/
- Componente: **Desktop development with C++**

### 2. Qt 6.11+
- https://www.qt.io/download-qt-installer
- Componente: `Qt 6.11.x → Prebuilt Components for MSVC 2022 64-bit`
- Path: `C:\Qt\6.11.x\msvc2022_64`

### 3. Git
- https://git-scm.com/download/win

### 4. Boost 1.85+
- https://www.boost.org/users/download/
- Extrair em `C:\local\boost_1_xx_0`
- Header-only — não compilar

### 5. libsodium 1.0.22 MSVC
- https://github.com/jedisct1/libsodium/releases → `libsodium-1.0.22-stable-msvc.zip`
- Extrair em `C:\local\libsodium`
- Lib: `C:\local\libsodium\x64\Release\v143\static\libsodium.lib`

### 6. FFmpeg BtbN shared
- https://github.com/BtbN/FFmpeg-Builds/releases → `ffmpeg-master-latest-win64-gpl-shared.zip`
- Extrair em `C:\local\ffmpeg`
- DLLs necessárias em `build/Release/`: `avcodec-63.dll`, `avutil-61.dll`, `swscale-10.dll`
- **Nota:** números de versão das DLLs podem variar — verificar em `C:\local\ffmpeg\bin\`

### 7. ADB platform-tools
- https://developer.android.com/studio/releases/platform-tools
- Copiar `adb.exe`, `AdbWinApi.dll`, `AdbWinUsbApi.dll` para `resources/adb/`

### 8. Softcam (git submodule)
```bash
git submodule add https://github.com/tshino/softcam third_party/softcam
git submodule update --init
```

---

## Build

```powershell
# VS 2026
& "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" `
    -B build -G "Visual Studio 18 2026" -A x64 `
    -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/msvc2022_64"

# VS 2022
& "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" `
    -B build -G "Visual Studio 17 2022" -A x64 `
    -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/msvc2022_64"

# Compilar
& "<path-do-cmake>" --build build --config Release
```

**Cache FetchContent:** mantido em `C:\local\cmake_deps` — não é apagado ao deletar `build/`.

---

## Registro do Softcam (uma vez, como Administrador)

```powershell
regsvr32 "C:\...\build\Release\softcam.dll"
```

Após isso "Softcam" aparece na lista de câmeras do Meet, Zoom, Teams e Discord.

---

## Protocolo WebSocket — mensagens implementadas

### Android → PC

| Tipo | Payload | Descrição |
|---|---|---|
| `hello` | `deviceName`, `deviceId` | Identificação ao conectar |
| `pair_request` | `pin`, `pk`, `deviceId`, `deviceName` | Solicita pareamento |
| `start_camera` | — | Inicia stream |
| `stop_camera` | — | Para stream |
| `ping` | — | Keepalive |

### PC → Android

| Tipo | Payload | Descrição |
|---|---|---|
| `welcome` | `deviceName`, `deviceId` | Confirmação de conexão |
| `pair_accepted` | — | Pareamento aceito |
| `pair_rejected` | — | PIN inválido |
| `pong` | — | Resposta ao ping |
| `error` | `message` | Erro genérico |

---

## Protocolo de stream TCP

Porta `45679`. Framing:

```
[4 bytes big-endian: tamanho N][N bytes: NALU H264]
[4 bytes big-endian: tamanho M][M bytes: NALU H264]
```

O PC aceita uma conexão por vez. Se o socket cair, aceita nova conexão automaticamente.

---

## Fase 2 — Transferência de arquivos

### O que implementar

Transferência em qualquer direção entre quaisquer dois dispositivos:
- Android → PC
- PC → Android
- Android → Android (via PC como relay)
- PC → PC (via protocolo direto)

Sem limite de tamanho, progresso em tempo real, qualquer formato.

### Protocolo — HTTP chunked

O **remetente** sobe um servidor HTTP simples em uma porta efêmera e anuncia ao
destinatário via WebSocket de controle. O **destinatário** faz GET e recebe o arquivo
em chunks.

**Fluxo:**

```
Remetente Destinatário
| |
|-- sobe HTTP server :porta_efem -->|
| |
|-- WS: file_offer ---------------->|
| {type: "file_offer", |
| fileId: "uuid", |
| fileName: "foto.jpg", |
| fileSize: 4200000, |
| mimeType: "image/jpeg", |
| port: 51234} |
| |
|<-- WS: file_accept ---------------|
| {type: "file_accept", |
| fileId: "uuid"} |
| |
|<-- HTTP GET /file/<uuid> ---------|
| |
|-- HTTP 200 chunked transfer ----->|
| [chunks de dados] |
| |
|-- WS: file_complete ------------->|
| {type: "file_complete", |
| fileId: "uuid"} |
```

**Mensagens WebSocket novas a adicionar:**

```cpp
// em MessageDispatcher.h — adicionar ao enum MsgType:
FileOffer,     // remetente → destinatário
FileAccept,    // destinatário → remetente
FileReject,    // destinatário → remetente
FileProgress,  // remetente → destinatário (opcional, progresso em %)
FileComplete,  // remetente → destinatário
FileError,     // qualquer direção
```

**Estrutura das mensagens:**

```json
// file_offer
{
  "type": "file_offer",
  "fileId": "uuid-v4",
  "fileName": "documento.pdf",
  "fileSize": 4200000,
  "mimeType": "application/pdf",
  "port": 51234
}

// file_accept
{
  "type": "file_accept",
  "fileId": "uuid-v4"
}

// file_reject
{
  "type": "file_reject",
  "fileId": "uuid-v4",
  "reason": "user_cancelled"
}

// file_progress (opcional)
{
  "type": "file_progress",
  "fileId": "uuid-v4",
  "bytesTransferred": 2100000,
  "percent": 50
}

// file_complete
{
  "type": "file_complete",
  "fileId": "uuid-v4"
}
```

### Arquivos novos a criar

```
src/core/
├── FileTransferServer.h / .cpp ← HTTP server (Boost.Beast HTTP)
├── FileTransferClient.h / .cpp ← HTTP client (Boost.Beast HTTP)
└── FileTransferManager.h / .cpp ← orquestra offer/accept/progress
```

**FileTransferServer** — usa Boost.Beast HTTP (já disponível via Boost.Beast que está
no projeto). Sobe listener em porta efêmera, serve o arquivo em chunks de 64KB,
notifica progresso via callback.

**FileTransferClient** — usa Boost.Beast HTTP client. Conecta ao IP:porta do remetente,
faz GET, salva em disco em chunks, notifica progresso via callback.

**FileTransferManager** — recebe eventos do WsServer (file_offer recebido), exibe
notificação no systray pedindo confirmação do usuário, inicia download se aceito,
notifica progresso.

### Integração com WsServer

```cpp
// WsServer.h — adicionar em ServerCallbacks:
std::function<void(uint64_t sessionId, const json& payload)> onFileOffer;
std::function<void(uint64_t sessionId, const json& payload)> onFileAccept;
std::function<void(uint64_t sessionId, const json& payload)> onFileReject;
std::function<void(uint64_t sessionId, const json& payload)> onFileComplete;
```

### UI no systray para transferência

**Receber arquivo:**
- Notificação balloon: "Pixel 7 quer enviar foto.jpg (4MB) — Aceitar?"
- Clique na notificação abre diálogo de confirmação
- Progresso em notificação balloon ou tooltip do ícone
- Ao completar: "foto.jpg recebido — Abrir pasta?"

**Enviar arquivo (PC → Android):**
- Menu do systray: "Enviar arquivo para <dispositivo>"
- Abre diálogo de seleção de arquivo nativo (`QFileDialog`)
- Progresso no menu ou janela principal

---

## Fase UI — Janela completa

### Abordagem

Systray permanece como interface primária. Duplo clique no ícone abre janela completa
com informações detalhadas.

### O que implementar

**Novo arquivo:** `src/ui/MainWindow.h / .cpp` — `QMainWindow` ou `QWidget`.

Aberto via:
```cpp
// TrayIcon.cpp — onActivated()
if (reason == QSystemTrayIcon::DoubleClick) {
    emit mainWindowRequested();
}
```

**Conteúdo da janela:**

```
┌─────────────────────────────────────────┐
│ Iriseus [─][×] │
├─────────────────────────────────────────┤
│ Dispositivos conectados │
│ ┌───────────────────────────────────┐ │
│ │ 📱 Pixel 7 [Câmera ✓] │ │
│ │ 192.168.1.50 USB │ │
│ └───────────────────────────────────┘ │
├─────────────────────────────────────────┤
│ Câmera │
│ Status: ● Ativa 1280×720 @ 30fps │
│ [Parar câmera] │
├─────────────────────────────────────────┤
│ Transferências │
│ foto.jpg ████████░░ 80% 2.1/4MB │
├─────────────────────────────────────────┤
│ [Parear novo dispositivo] [Configurações] │
└─────────────────────────────────────────┘
```

**Fechar a janela não fecha o app** — apenas esconde. App continua no systray.
Implementar com `closeEvent` override:

```cpp
void MainWindow::closeEvent(QCloseEvent* event)
{
    hide();
    event->ignore();
}
```

### Configurações (futura)

- Pasta de download padrão
- Resolução da câmera
- Iniciar com o Windows (entrada no registro)
- Desregistrar/registrar Softcam

---

## Ordem de implementação sugerida

1. **Mensagens de transferência no MessageDispatcher** — adicionar enums e strings
2. **FileTransferServer** — HTTP chunked server com Boost.Beast
3. **FileTransferClient** — HTTP chunked client com Boost.Beast
4. **FileTransferManager** — orquestra, integra com WsServer callbacks
5. **Notificações no systray** — balloon para offer/accept/complete
6. **Menu systray** — adicionar "Enviar arquivo para..."
7. **MainWindow** — janela completa com lista de dispositivos, status câmera,
   progresso de transferências
8. **TrayIcon** — duplo clique abre MainWindow

---

## Notas importantes para o próximo contexto

- Boost.Beast já está no projeto e já é usado para WebSocket — reutilizar para HTTP
- O WsServer roda em thread dedicada (Asio io_context) separada da thread Qt
- Qualquer callback que toque em UI deve usar `QMetaObject::invokeMethod(..., Qt::QueuedConnection)`
- libsodium está linkado estaticamente com `SODIUM_STATIC` definido
- softcam.dll é carregada via `LoadLibrary` em runtime — não linkada estaticamente
- ADB usa porta `5038` e path absoluto para evitar conflito com ADB do usuário
- FetchContent cache em `C:\local\cmake_deps` — preservar ao limpar build
- FFmpeg DLLs precisam estar na pasta do exe — copiadas automaticamente pelo CMake
  mas os números de versão das DLLs podem variar entre builds do FFmpeg

