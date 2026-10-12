# Plano — Transferência de Arquivos (Desktop / Windows)

> Plano do lado **Desktop**. O lado mobile está em `Iriseus-Mobile/plano.md`. A seção de protocolo é **idêntica** nos dois arquivos: qualquer mudança precisa ser feita nos dois.

Objetivo: transferir qualquer tipo de arquivo, em qualquer direção (PC ⇄ celular), por WiFi ou USB.

**Decisões fechadas**
- Pasta de destino: `%USERPROFILE%\Downloads\Iriseus`
- Ofertas de dispositivos pareados são **aceitas automaticamente**. Não pareado = recusa.
- **Sem retomada**: se a conexão cair, a transferência falha, o `.part` é apagado e o usuário reenvia.

---

## 1. Protocolo (compartilhado)

```
           Controle (JSON)  ── WebSocket :45678 (já existe)
[Mobile] ◄──────────────────────────────────────────────► [Desktop]
           Dados (binário)  ── TCP :45680 (novo, criptografado)
```

- O desktop é **sempre servidor** no 45680 e o mobile **sempre cliente**, nos dois sentidos. No USB basta `adb reverse tcp:45680 tcp:45680`.
- Chave de arquivos: `crypto_kdf_derive_from_key(subkey_id=1, ctx="irisfile", key=segredo_ECDH_do_pareamento)`.
- Cifra: `crypto_secretstream_xchacha20poly1305` (libsodium). Integridade final por SHA-256 do arquivo.

### 1.1 Mensagens WS

| `type` | Direção | Campos |
|---|---|---|
| `file_offer` | remetente → destinatário | `transferId` (uuid v4), `name`, `size` (uint64), `mime`, `sha256` (hex) |
| `file_accept` | destinatário → remetente | `transferId` |
| `file_reject` | destinatário → remetente | `transferId`, `reason` (`not_paired`, `no_space`, `busy`) |
| `file_ready` | desktop → mobile | `transferId`, `port`, `token` (32 bytes, base64) |
| `file_complete` | destinatário → remetente | `transferId`, `ok` (bool), `error?` |
| `file_cancel` | qualquer um | `transferId` |

### 1.2 Canal de dados (TCP 45680)

```
Handshake (mobile → desktop):
  [16 bytes transferId (uuid binário)][32 bytes token][1 byte dir: 0 = mobile envia, 1 = desktop envia]

Quem envia os dados:
  [24 bytes header secretstream]
  [4 bytes BE: N][N bytes chunk cifrado]  (plaintext ≤ 256 KiB, +17 bytes de tag)
  ...  (último chunk com TAG_FINAL)
```

### 1.3 Fluxos
- **Celular → PC**: `file_offer` (M) → `file_accept` (D, automático) → `file_ready` (D) → M conecta e envia → D confere o hash → `file_complete` (D).
- **PC → Celular**: `file_offer` (D) → `file_accept` (M, automático) → `file_ready` (D) → M conecta (dir=1) e recebe → M confere o hash → `file_complete` (M).
- Qualquer erro, queda ou timeout de 30 s sem dados → `file_cancel`/falha, e o `.part` é apagado.

---

## 2. Tarefas — Desktop

### Marco 1 — Celular → PC sem criptografia
- [x] **D1.1** Registrar as mensagens `file_*` no [`MessageDispatcher.h`](file:///c:/Users/Plusb/Documents/Iriseus/src/core/MessageDispatcher.h) e no roteamento de `WsSession`.
- [x] **D1.2** `src/core/FileTransferProtocol.h`: constantes (porta 45680, chunk 256 KiB, timeout 30 s), structs `FileOffer`/`TransferState`, (de)serialização com nlohmann.
- [x] **D1.3** `src/core/FileTransferServer.{h,cpp}`: acceptor Boost.Asio no 45680, em thread própria (padrão do `StreamReceiver`). Valida o handshake (`transferId` + `token`) e fecha conexões desconhecidas.
- [x] **D1.4** Recebimento: lê os frames, grava em `Downloads\Iriseus\<nome>.part`, SHA-256 incremental (`crypto_hash_sha256_*`).
- [x] **D1.5** Finalização: confere o hash, renomeia `.part` → final (colisão vira `nome (1).ext`) e envia `file_complete`.
- [x] **D1.6** `src/core/FileTransferManager.{h,cpp}` (QObject): estado por `transferId`, auto-accept se o dispositivo estiver pareado, gera o `token` e manda `file_ready`. Sinais `progress`, `finished`, `failed`.
- [x] **D1.7** Integrar o manager ao `Application`.

### Marco 2 — Criptografia
- [ ] **D2.1** Expor `deriveFileKey()` em `PairingCrypto` (KDF da seção 1).
- [ ] **D2.2** Aplicar `secretstream_pull` no recebimento. Tag inválida = falha imediata.
- [ ] **D2.3** Testar a compatibilidade com o mobile (vetor de teste fixo: chave + arquivo pequeno).

### Marco 3 — PC → Celular
- [ ] **D3.1** Modo envio no `FileTransferServer`: lê o arquivo em blocos, faz `secretstream_push` e escreve os frames com escrita assíncrona.
- [ ] **D3.2** O manager calcula o SHA-256 antes de mandar o `file_offer` (em thread de trabalho, para arquivos grandes).
- [ ] **D3.3** Fila simples: 1 transferência ativa por vez, as demais aguardam.

### Marco 4 — UI
- [ ] **D4.1** Systray: **"Enviar arquivo…"** (`QFileDialog::getOpenFileNames`) e **"Abrir pasta de recebidos"**.
- [ ] **D4.2** Notificação ao concluir o recebimento (`QSystemTrayIcon::showMessage`). Clicar abre a pasta.
- [ ] **D4.3** `src/ui/TransfersWindow.{h,cpp}`: lista com nome, progresso, velocidade e cancelar. Aceita drag & drop para enviar.

### Marco 5 — USB e casos de borda
- [ ] **D5.1** `AdbManager`: adicionar `adb reverse tcp:45680 tcp:45680`.
- [ ] **D5.2** Checar espaço livre (`QStorageInfo`) antes de aceitar → `file_reject{no_space}`.
- [ ] **D5.3** Sanitizar o nome (`..`, `\ / : * ? " < > |`, nomes reservados `CON`, `NUL`, `COM1`…).
- [ ] **D5.4** Cancelamento/queda/timeout: fechar o socket e apagar o `.part`.
- [ ] **D5.5** Regra de firewall para o 45680, junto com as existentes.
- [ ] **D5.6** Transferir com a câmera ativa → o vídeo não pode degradar.

---

## 3. Testes de aceitação (desktop)
- [ ] Arquivos de 0 B, 1 B, 1 MB e > 4 GB (tamanhos em `uint64_t`).
- [ ] Nomes com acentos, emoji e espaços. Nome malicioso (`..\..\x.dll`) sanitizado.
- [ ] O hash confere nos dois sentidos.
- [ ] Cancelar ou derrubar a rede no meio → nenhum arquivo órfão e erro claro na UI.
- [ ] Token inválido no 45680 → conexão recusada.
- [ ] Modo USB nos dois sentidos.
