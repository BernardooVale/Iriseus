# Iriseus — Roteiro de Implementação do App Flutter Android

Este documento é um roteiro completo para implementar o app Flutter Android do Iriseus
em um novo contexto. Leia tudo antes de começar.

---

## Contexto

O app Windows já está implementado e funcional. Ele:
- Anuncia `_iriseus._tcp.local` via mDNS
- Aceita conexões WebSocket na porta `45678`
- Aceita stream H264 via socket TCP na porta `45679`
- Implementa pareamento ECDH X25519 + TOFU via QR Code ou PIN
- Decodifica H264 com FFmpeg e envia frames para Softcam (webcam virtual DirectShow)

O app Flutter é o lado Android — captura câmera, envia stream, gerencia pareamento.

---

## Stack Flutter

| Componente | Pacote |
|---|---|
| WebSocket cliente | `web_socket_channel` |
| mDNS discovery | `nsd` ou `multicast_dns` |
| QR Code scanner | `mobile_scanner` |
| QR Code gerador | não necessário (QR é gerado pelo PC) |
| Câmera | `camera` + `camerax` (ou direto via platform channel) |
| H264 encoding | `flutter_webrtc` (só MediaCodec) ou platform channel nativo |
| Criptografia ECDH | `cryptography` (dart) ou platform channel com libsodium |
| Persistência de pareamento | `shared_preferences` |
| Permissões | `permission_handler` |

**Recomendação de encoding:** implementar via platform channel Android nativo
(Kotlin/Java) usando `CameraX` + `MediaCodec` — mais controle sobre o H264 e menor
latência que qualquer pacote Flutter puro.

---

## Estrutura de pastas sugerida

```
iriseus/
├── android/
│ └── app/src/main/kotlin/com/iriseus/app/
│ ├── CameraStreamService.kt ← CameraX + MediaCodec + socket TCP
│ └── MainActivity.kt
├── lib/
│ ├── main.dart
│ ├── core/
│ │ ├── ws_client.dart ← WebSocket de controle
│ │ ├── mdns_discovery.dart ← descoberta de dispositivos
│ │ ├── pairing_manager.dart ← ECDH + TOFU + persistência
│ │ └── stream_controller.dart ← coordena camera + socket
│ ├── ui/
│ │ ├── home_screen.dart ← lista de dispositivos descobertos
│ │ ├── pairing_screen.dart ← scanner QR + entrada PIN
│ │ └── camera_screen.dart ← preview câmera + controles
│ └── models/
│ ├── device.dart
│ └── pairing_info.dart
```

---

## Protocolo de controle — WebSocket

Porta: `45678`
Formato: JSON, texto

### Mensagens Android → PC

#### hello (após conectar)
```json
{
  "type": "hello",
  "deviceName": "Pixel 7",
  "deviceId": "uuid-gerado-uma-vez"
}
```

#### pair_request (durante pareamento)
```json
{
  "type": "pair_request",
  "pin": "123456",
  "pk": "<chave-publica-X25519-base64-urlsafe-sem-padding>",
  "deviceId": "uuid-gerado-uma-vez",
  "deviceName": "Pixel 7"
}
```

#### start_camera
```json
{"type": "start_camera"}
```

#### stop_camera
```json
{"type": "stop_camera"}
```

#### ping (keepalive)
```json
{"type": "ping"}
```

### Mensagens PC → Android

#### welcome (resposta ao hello)
```json
{
  "type": "welcome",
  "deviceName": "Iriseus PC",
  "deviceId": "pc-placeholder-id"
}
```

#### pair_accepted
```json
{"type": "pair_accepted"}
```

#### pair_rejected
```json
{"type": "pair_rejected"}
```

#### pong (resposta ao ping)
```json
{"type": "pong"}
```

---

## Protocolo de stream — TCP

Porta: `45679`
Direção: Android → PC

### Framing

Cada NALU H264 é prefixado com seu tamanho em 4 bytes big-endian:

```
[0x00][0x00][0x04][0xA2] ← tamanho do NALU em big-endian (ex: 1186 bytes)
[...NALU H264 data...]
[0x00][0x00][0x02][0x10] ← próximo NALU
[...NALU H264 data...]
```


O PC lê exatamente 4 bytes, interpreta como uint32 big-endian, depois lê exatamente
N bytes do NALU. Nunca enviar NALUs sem o prefixo de tamanho.

### Configuração do encoder

- Codec: `video/avc` (H264)
- Resolução: 1280x720 (negociável futuramente)
- Framerate: 30fps
- Bitrate: ~2Mbps
- Profile: Baseline (mais compatível com FFmpeg decoder do PC)
- Key frame interval: 1 segundo

### Ordem de operação

1. Conectar socket TCP ao IP do PC na porta `45679`
2. Iniciar CameraX preview
3. Configurar MediaCodec encoder
4. Para cada frame da câmera:
   - Submeter ao MediaCodec
   - Para cada NALU de saída do MediaCodec:
     - Escrever 4 bytes big-endian com o tamanho
     - Escrever o NALU
5. Ao parar: fechar socket, parar MediaCodec, parar CameraX

---

## Fluxo de pareamento

### Via QR Code

1. Usuário abre "Parear novo dispositivo" no PC → PC exibe QR Code
2. App Android abre scanner de QR
3. QR contém payload JSON:
```json
{
  "v": 1,
  "ip": "192.168.1.100",
  "port": 45678,
  "pin": "123456",
  "pk": "<chave-publica-X25519-do-PC-base64-urlsafe>"
}
```
4. App Android:
   - Gera seu próprio par de chaves X25519 efêmero
   - Conecta WebSocket em `ws://ip:port`
   - Envia `hello`
   - Envia `pair_request` com PIN + chave pública própria
5. PC responde `pair_accepted` ou `pair_rejected`
6. Se aceito: ambos derivam segredo compartilhado ECDH
7. Salvar `deviceId` do PC + chave pública do PC em `shared_preferences`

### Via PIN

Mesmo fluxo, mas o usuário digita o PIN manualmente em vez de escanear QR.
O app ainda precisa saber o IP do PC — descoberto via mDNS.

### Reconexão automática

Se `deviceId` do PC já está salvo em `shared_preferences`:
- Ao descobrir o PC via mDNS, conectar automaticamente sem novo pareamento
- Enviar `hello` — PC reconhece o `deviceId` e aceita

---

## Descoberta mDNS

O PC anuncia `_iriseus._tcp.local` na rede.

No Android, usar o pacote `nsd` (Network Service Discovery nativo do Android via
platform channel) ou `multicast_dns` (dart puro).

**Dados esperados no registro mDNS:**
- Service type: `_iriseus._tcp`
- TXT record: `version=1`
- SRV record: hostname + porta `45678`

O app deve listar todos os dispositivos descobertos na home screen com nome e IP,
permitindo o usuário selecionar qual conectar.

---

## Modo USB

Quando conectado via USB com ADB:
- O app Windows roda `adb reverse tcp:45678 tcp:45678` e `adb reverse tcp:45679 tcp:45679`
- O app Android deve detectar que está em modo USB e usar `127.0.0.1` como IP
- Detecção: tentar conexão em `127.0.0.1:45678` — se conectar, está em modo USB

**Nota:** o ADB reverse é configurado automaticamente pelo app Windows ao detectar
o celular via USB. O app Flutter não precisa fazer nada especial além de tentar
`127.0.0.1` primeiro.

---

## Criptografia

Biblioteca recomendada: pacote `cryptography` do Dart, ou platform channel com
libsodium via FFI.

**Algoritmo:** X25519 (Curve25519 ECDH)
**Derivação:** XOR de rx e tx keys (mesma lógica do lado PC com `crypto_kx_server_session_keys`)
**Encoding:** base64 urlsafe sem padding

O app Android é o "client" no handshake:
```dart
// Pseudocódigo
final keyPair = await X25519().newKeyPair();
final pubKeyBytes = await keyPair.extractPublicKey();
final pubKeyB64 = base64UrlEncode(pubKeyBytes.bytes)
    .replaceAll('=', ''); // sem padding

// Após receber pk do PC via QR:
final pcPubKey = SimplePublicKey(base64UrlDecode(pcPkB64), type: KeyPairType.x25519);
final sharedSecret = await X25519().sharedSecretKey(
    keyPair: keyPair,
    remotePublicKey: pcPubKey,
);
```

---

## Permissões Android necessárias

No `AndroidManifest.xml`:
```xml
<uses-permission android:name="android.permission.CAMERA" />
<uses-permission android:name="android.permission.INTERNET" />
<uses-permission android:name="android.permission.ACCESS_NETWORK_STATE" />
<uses-permission android:name="android.permission.ACCESS_WIFI_STATE" />
<uses-permission android:name="android.permission.CHANGE_WIFI_MULTICAST_STATE" />
```

A última é obrigatória para mDNS funcionar no Android.

---

## Telas do app

### HomeScreen
- Lista de dispositivos descobertos via mDNS
- Botão "Conectar via USB" (tenta 127.0.0.1)
- Botão "Parear novo dispositivo"
- Status de conexão atual

### PairingScreen
- Scanner de QR Code (câmera traseira)
- Campo para entrada manual de PIN
- Feedback de sucesso/erro

### CameraScreen
- Preview da câmera (frente ou traseira, configurável)
- Botão de alternar câmera
- Indicador de status do stream (conectado/desconectado)
- Indicador de FPS e latência (opcional)
- Botão de desconectar

---

## Notas importantes

- O app Windows é o **servidor** — WebSocket e TCP socket ficam no PC, Flutter conecta
- O pareamento invalida o PIN após uso — não reutilizar
- Manter keepalive via `ping`/`pong` a cada 30 segundos
- Se o socket TCP cair, tentar reconectar automaticamente sem novo pareamento
- O encoder H264 deve enviar SPS/PPS antes do primeiro frame — o FFmpeg do PC precisa
  deles para inicializar o decoder
- Testar com `adb logcat` para ver logs do MediaCodec em caso de problemas de encoding

---

## Ordem de implementação sugerida

1. WebSocket client — conectar, enviar `hello`, receber `welcome`
2. Pareamento QR — scanner, parse do payload, `pair_request`, persistência
3. mDNS discovery — listar PCs na rede
4. Pipeline de câmera — CameraX + MediaCodec + socket TCP
5. UI polida — HomeScreen, CameraScreen, transições
6. Modo USB — detecção automática de `127.0.0.1`
7. Reconexão automática