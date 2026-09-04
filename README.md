# Iriseus

**Iris + Caduceus**

Plataforma de comunicação local entre dispositivos — celular ou computador — via WiFi ou USB, sem dependência de serviços externos, sem configuração manual, sem software intermediário.

---

## Problema original

Necessidade de usar o celular como webcam em entrevistas técnicas em empresas grandes, que exigem câmera ligada durante provas online. O PC não possui câmera integrada.

A solução anterior (WebcamStream) resolvia o problema mas exigia OBS + nginx + Python + ADB instalados e configurados manualmente. O Iriseus elimina todas essas dependências.

---

## Estado atual

### App Windows — implementado

- Systray com menu (iniciar/parar câmera, parear dispositivo, sair)
- Servidor WebSocket na porta `45678` — canal de controle
- Descoberta de rede via mDNS (`_iriseus._tcp.local`)
- Pareamento via QR Code + PIN de 6 dígitos (ECDH X25519 + TOFU)
- Modo USB via ADB bundlado, porta isolada `5038`, `adb reverse` automático
- Pipeline de câmera: socket TCP porta `45679` → FFmpeg H264 decode → RGB24 → Softcam → DirectShow
- Webcam virtual via Softcam (DirectShow filter, MIT)

### App Android — pendente

### Fases 2–3 — pendentes

---

## Arquitetura

```
[Flutter Android]
↓ H264 via socket TCP (WiFi / USB via ADB reverse)
[App Windows — recebe e decodifica stream]
↓
[Softcam — DirectShow filter registrado no sistema]
↓ scSendFrame()
[Meet / Zoom / Teams — enxerga como webcam nativa]
```

**Protocolo de framing do stream:**

```
[4 bytes big-endian: tamanho N][N bytes: NALU H264]
[4 bytes big-endian: tamanho M][M bytes: NALU H264]
```

**Portas:**
- `45678` — WebSocket de controle
- `45679` — stream H264 TCP
- `5353` — mDNS (multicast)
- `5038` — servidor ADB interno (isolado)

---

## Stack

| Componente | Tecnologia |
|---|---|
| App mobile | Flutter (Android hoje, iOS futuro) |
| App desktop | C++ nativo + Qt 6 (systray) |
| Build system | CMake 3.24+ |
| WebSocket | Boost.Beast + Boost.Asio |
| JSON | nlohmann/json |
| mDNS | mjansson/mdns |
| QR Code | nayuki/QR-Code-generator |
| Criptografia | libsodium (X25519 ECDH) |
| Decodificação H264 | FFmpeg (BtbN shared build) |
| Webcam virtual | Softcam (DirectShow, MIT) |
| Descoberta | mDNS — `nsd` Android, `mdns.h` Windows |
| USB | ADB bundlado, isolado, transparente |

---

## Dependências para build

### 1. Visual Studio 2022 ou 2026
- Download: https://visualstudio.microsoft.com/downloads/
- Componente obrigatório: **Desktop development with C++**

### 2. Qt 6.11+
- Download: https://www.qt.io/download-qt-installer
- Componente: `Qt 6.11.x → Prebuilt Components for MSVC 2022 64-bit`
- Path padrão: `C:\Qt\6.11.x\msvc2022_64`

### 3. Git
- Download: https://git-scm.com/download/win

### 4. Boost 1.85+
- Download: https://www.boost.org/users/download/
- Extrair em: `C:\local\boost_1_xx_0`
- Header-only — não precisa compilar

### 5. libsodium 1.0.22 (MSVC)
- Download: https://github.com/jedisct1/libsodium/releases → `libsodium-1.0.22-stable-msvc.zip`
- Extrair em: `C:\local\libsodium`
- Path da lib: `C:\local\libsodium\x64\Release\v143\static\libsodium.lib`

### 6. FFmpeg (BtbN shared build)
- Download: https://github.com/BtbN/FFmpeg-Builds/releases → `ffmpeg-master-latest-win64-gpl-shared.zip`
- Extrair em: `C:\local\ffmpeg`

### 7. ADB platform-tools
- Download: https://developer.android.com/studio/releases/platform-tools
- Copiar `adb.exe`, `AdbWinApi.dll`, `AdbWinUsbApi.dll` para `resources/adb/`

### 8. Softcam (git submodule)
```bash
git submodule add https://github.com/tshino/softcam third_party/softcam
git submodule update --init
```

---

## Build

```powershell
# Configurar (ajuste o path do Qt e o generator conforme sua versão do VS)
& "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" `
    -B build -G "Visual Studio 18 2026" -A x64 `
    -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/msvc2022_64"

# Compilar
& "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" `
    --build build --config Release
```

**Nota:** Se estiver usando VS 2022, substituir `Visual Studio 18 2026` por `Visual Studio 17 2022`.

### Cache de dependências

Para evitar re-download do FetchContent ao recriar a pasta `build/`, o cache é mantido em `C:\local\cmake_deps`. Pode ser adicionado ao `.gitignore`:

```
build/
```

---

## Primeiro uso

### Registrar Softcam (necessário uma vez, como Administrador)

```powershell
regsvr32 "C:\Users\<usuario>\Documents\Iriseus\build\Release\softcam.dll"
```

Após isso, "Softcam" aparece como câmera disponível no Meet, Zoom, Teams e Discord.

---

## Ordem de desenvolvimento

1. ✅ Protocolo base — mDNS + WebSocket + pareamento QR/PIN
2. ✅ Streaming de câmera + Softcam — H264 via socket TCP, scSendFrame()
3. ✅ Modo USB — ADB bundlado, porta isolada, adb reverse automático
4. 🔲 App Flutter Android
5. 🔲 Installer (NSIS) — registra Softcam, copia DLLs, entrada no Startup
6. 🔲 Transferência de arquivos
7. 🔲 Microfone virtual
8. 🔲 Área de transferência, notificações
9. 🔲 Compartilhamento de tela