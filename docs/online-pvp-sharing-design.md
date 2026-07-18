# Online PvP Save Sharing Design

Date: 2026-07-18  
Status: Draft for implementation

## Overview

This feature extends the completed offline PvP save workflow with a small online
repository. A player can publish a verified PvP checkpoint, receive a server-assigned
share code, send that code to another player, and download an opponent directly from
the game.

The service is intentionally not a game authority. It stores opaque compressed files,
assigns collision-free identifiers, applies proof-of-work and operational limits, and
returns files by share code. The exporting client verifies the final packaged save
before upload, and the importing client performs the existing strict PvP verification
after download.

The Rust server source lives in this repository. Development and tests may run natively
under WSL Ubuntu 24.04, while release builds use the static
`x86_64-unknown-linux-musl` target so the executable can be copied into the existing
nginx container without depending on its libc or installed packages.

## MVP Decisions

- Use direct lookup by a server-assigned share code.
- Do not provide a public recent-upload browser in the initial version.
- Do not ask players to name uploads.
- Display an upload as `對手存檔 · <share code>` in the client.
- Do not implement accounts, player identity, publisher signatures, replacement, or
  deletion in the initial version.
- Expire uploads automatically according to server configuration.
- Require proof-of-work for every new upload.
- Do not attempt to prove that a request came from an official, unmodified client.
- Do not validate chess rules or decompress uploads on the server.
- Continue to verify saves on the client before upload and after download.
- Use the current exact-game-version PvP compatibility rule.
- Use HTTPS externally, terminated by the existing nginx deployment.
- Keep the Rust service on a loopback HTTP port inside the nginx container.

Public discovery, optional labels, persistent publisher identity, and owner-controlled
deletion can be added without changing the stored package format.

## Goals

- Publish and download PvP saves from Windows, Android, and browser builds.
- Reuse the completed `ChessPvpSaveVerifier` and checkpoint serialization.
- Keep platform-specific networking behind one client abstraction.
- Keep proof-of-work deterministic, cancellable, and testable in common C++ code.
- Prevent filename clashes by accepting no client-controlled storage names.
- Avoid embedded shared secrets or certificates.
- Bound server storage, upload bandwidth, and challenge reuse.
- Produce one static Rust executable that can be copied into the existing deployment.
- Keep the protocol versioned independently from the game and package versions.

## Non-goals

- Proving that a human played the replay.
- Proving that the caller is an official or unmodified game binary.
- Preventing automation from producing valid replay actions.
- Server-side replay or game-state verification.
- Rankings, matchmaking, brackets, ratings, or results reporting.
- Public upload browsing in the MVP.
- Accounts, password recovery, cross-device ownership, or unique player names.
- Resumable uploads.
- Object storage or an external database.
- Bundling or redistributing nginx with the service.

## Trust and Security Model

The API protocol is public. CORS, request headers, binary obfuscation, or a secret
embedded in the game are not treated as authentication.

The following properties are provided by different layers:

| Property | Provider |
|---|---|
| Server identity and transport encryption | nginx HTTPS |
| Upload work requirement | Server challenge and SHA-256 proof-of-work |
| Transport and storage integrity | Server-calculated archive SHA-256 |
| Package structure and decompression safety | Downloading client |
| Legal replay and matching final state | `ChessPvpSaveVerifier` on each client |
| Storage and bandwidth bounds | nginx and Rust service limits |

An attacker can upload a structurally invalid or game-invalid file after completing the
required proof-of-work. This is accepted by design. Such a file is rejected by every
normal client after download. Direct share-code lookup prevents invalid uploads from
polluting a public browser, while rate limits, expiry, size limits, and proof-of-work
bound the server impact.

SHA-256 proves that the downloaded bytes match the bytes accepted by the service. It
does not prove that the file contains a valid save. The strict client verifier remains
authoritative for gameplay.

## High-level Flow

### Publish

1. Capture a direct `ChessSessionCheckpoint` from the current Hard-mode session.
2. Serialize it as UTF-8 JSON.
3. Build a `.kyspvp` package.
4. Reopen the completed package and run all package checks locally.
5. Run `ChessPvpSaveVerifier` against the extracted checkpoint JSON.
6. Calculate the SHA-256 and byte size of the final package.
7. Request an upload challenge from the server.
8. Cooperatively solve the proof-of-work challenge with visible progress and
   cancellation.
9. Submit the solution counter and receive a short-lived upload authorization.
10. Upload the exact package bytes with that authorization.
11. Store the returned publication receipt locally.
12. Show and copy the assigned share code.

### Download

1. Normalize the entered share code.
2. Download the `.kyspvp` package to temporary memory or app-private storage.
3. Enforce the client compressed-size limit while downloading.
4. Verify the server-provided archive SHA-256.
5. Open the archive with strict entry and decompression limits.
6. Validate `manifest.json` and the checkpoint SHA-256.
7. Run `ChessPvpSaveVerifier` cooperatively on `checkpoint.json`.
8. Use the resulting `ChessPvpComposition` only after verification succeeds.
9. Do not import the downloaded checkpoint into a numbered campaign save slot.

## PvP Package Format

### File type

- Suggested extension: `.kyspvp`
- Media type: `application/vnd.kys.pvp+zip`
- Container: ZIP
- Text encoding: UTF-8
- Package format version: `1`

The existing `libzip` dependency is used on Windows, Android, and WASM. The initial
format uses only ZIP Store or Deflate compression. Encrypted entries, split archives,
symlinks, directories, and unknown entries are rejected.

### Required entries

The archive contains exactly two regular files at its root:

```text
manifest.json
checkpoint.json
```

`checkpoint.json` is the direct JSON representation produced by
`ChessSessionCheckpoint::serializeJson()`. A full numbered-slot save envelope is not
written into the online package, although the existing offline importer may continue
to accept that envelope separately.

### Manifest schema

```json
{
  "format_version": 1,
  "game_version": "1.4.0",
  "checkpoint_entry": "checkpoint.json",
  "checkpoint_size_bytes": 123456,
  "checkpoint_sha256": "64 lowercase hexadecimal characters"
}
```

Rules:

- Unknown keys are rejected for package format version 1.
- `checkpoint_entry` must equal `checkpoint.json`.
- `checkpoint_size_bytes` is the exact uncompressed byte length.
- `checkpoint_sha256` hashes the exact UTF-8 bytes stored in `checkpoint.json`.
- `game_version` must match the checkpoint and running client.
- The manifest does not contain the archive hash because that would be circular.
- The archive hash is calculated over the final `.kyspvp` bytes and recorded by the
  service.

### Client archive limits

The package reader must enforce all of the following before allocating or extracting
large buffers:

- Maximum compressed archive bytes.
- Maximum number of ZIP entries.
- Exact allowed entry names.
- No duplicate entry names.
- Maximum uncompressed size per entry.
- Maximum combined uncompressed size.
- Supported compression methods only.
- No encrypted entries or filesystem-special entries.

The exact byte limits must be selected after measuring real long-run PvP checkpoints.
They are shared constants in the client and must not be lower than the server upload
limit. The server limit remains separately configurable so operations can lower it
without releasing a new client.

## HTTP Protocol

### Base path

```text
/pvp-api/v1
```

The browser uses a same-origin relative URL. Windows and Android receive the HTTPS base
URL from build configuration. Protocol version `v1` is independent of package
`format_version` and the game version.

All JSON uses UTF-8 and `application/json`. Timestamps use UTC RFC 3339 strings. Hashes
use lowercase hexadecimal. Identifiers and enum-like values are ASCII.

### Health check

```http
GET /pvp-api/healthz
```

Successful response:

```json
{
  "status": "ok",
  "service_version": "0.1.0"
}
```

This endpoint is not proof-of-work protected and must be inexpensive. Deployment health
checking should access it through nginx so both routing and the Rust process are tested.

### Create upload challenge

```http
POST /pvp-api/v1/upload-challenges
Content-Type: application/json
```

Request:

```json
{
  "protocol_version": 1,
  "format_version": 1,
  "game_version": "1.4.0",
  "archive_size_bytes": 345678,
  "archive_sha256": "64 lowercase hexadecimal characters"
}
```

The server checks protocol syntax, configured game-version policy, size limits, source
rate limits, and the archive hash representation. It does not receive or inspect the
archive yet.

Successful response:

```json
{
  "challenge_id": "019f745f-db08-7ce2-8ef9-1b7be36eccc0",
  "nonce": "base64url without padding",
  "algorithm": "sha256-leading-zero-v1",
  "difficulty_bits": 21,
  "expires_at": "2026-07-18T12:05:00Z"
}
```

Challenge rules:

- `challenge_id` is a server-generated UUID.
- `nonce` represents 32 cryptographically random bytes.
- The challenge is bound to the requested archive hash, size, format version, and game
  version in the server database.
- The challenge has a short configurable lifetime.
- Challenge creation is independently rate limited because no proof exists yet.
- A challenge may create at most one save record.

### Proof-of-work definition

For an unsigned 64-bit counter, construct this exact UTF-8 byte sequence:

```text
kys-pvp-upload-v1\n
<challenge_id>\n
<nonce>\n
<archive_sha256>\n
<archive_size_bytes in base-10>\n
<counter in base-10>
```

There is no final newline after the counter. All values are exactly those returned by
or submitted to the server. The archive hash is lowercase hexadecimal and the nonce is
unchanged base64url text.

The solution is valid when:

```text
SHA256(proof_input)
```

has at least `difficulty_bits` consecutive zero bits starting from the most significant
bit of byte zero. The test must count full zero bytes followed by the high zero bits of
the next byte.

The common C++ client and Rust service must share fixed protocol test vectors for:

- Proof input construction.
- SHA-256 output.
- Leading-zero-bit counting.
- Boundary difficulties at 0, 1, 7, 8, 9, 255, and 256 bits.
- Counter parsing and overflow rejection.

The production difficulty is configured on the server. It should initially be selected
from benchmarks targeting a short but noticeable solve on supported Android and WASM
devices, rather than selecting a bit count by assumption.

### Submit proof-of-work solution

```http
POST /pvp-api/v1/upload-challenges/{challenge_id}/solution
Content-Type: application/json
```

Request:

```json
{
  "counter": "1234567"
}
```

The counter is a decimal string rather than a JSON number so the protocol can represent
the full unsigned 64-bit range without JavaScript number precision loss.

The server validates the counter against the stored challenge before any archive body
is uploaded. A successful response authorizes exactly that challenge, archive hash, and
archive size:

```json
{
  "upload_token": "base64url without padding",
  "upload_expires_at": "2026-07-18T12:10:00Z"
}
```

The upload token represents 32 cryptographically random bytes. The database stores only
its SHA-256. Repeating the same valid solution before upload rotates the authorization:
the server returns a fresh token for the same challenge and invalidates the earlier
token. This recovers from a lost solution response without creating another upload
identity or storing bearer tokens in plaintext. Invalid solution attempts are rate
limited and counted against the challenge.

### Upload archive

```http
POST /pvp-api/v1/upload-challenges/{challenge_id}/archive
Content-Type: application/vnd.kys.pvp+zip
X-Kys-Upload-Token: base64url-without-padding

<raw .kyspvp bytes>
```

The browser is not required to set `Content-Length` manually. If present, nginx and the
service compare it with the challenge size. The service always counts the bytes it
actually receives.

Server processing order:

1. Load the challenge and check authorization expiry and state.
2. Verify the upload-token hash before accepting the full body.
3. Stream the body into a unique temporary file while calculating SHA-256.
4. Stop immediately if the configured byte limit or declared challenge size is
   exceeded.
5. Require exact byte-size and SHA-256 agreement with the challenge.
6. Generate an upload UUID and unique share code.
7. Atomically move the temporary file into permanent storage.
8. Insert the active save metadata and attach the receipt to the completed challenge.
9. Return the receipt.

Successful first response uses HTTP `201 Created`:

```json
{
  "upload_id": "019f7465-a851-7fd5-b627-aeb13bd62222",
  "share_code": "7K3M-R9FD-W2HX",
  "protocol_version": 1,
  "format_version": 1,
  "game_version": "1.4.0",
  "archive_size_bytes": 345678,
  "archive_sha256": "64 lowercase hexadecimal characters",
  "created_at": "2026-07-18T12:01:00Z",
  "expires_at": "2026-09-16T12:01:00Z"
}
```

Upload retries are idempotent by `challenge_id` and upload token. If the server committed
the upload but the response was lost, repeating the same request returns the same receipt
with HTTP `200 OK`. A challenge never produces a second save or share code. Completed
challenge rows are retained for a configurable idempotency window after the pending
challenge expiry would otherwise have elapsed.

Failed archive hash or length checks do not permit the challenge to be rebound to other
bytes. The service limits failed attempts per challenge to prevent unlimited uploads
against one solved proof.

### Download by share code

```http
GET /pvp-api/v1/saves/{share_code}
```

Share-code input is case-insensitive. The server removes ASCII hyphens before lookup and
rejects every other unexpected character. The canonical response code is uppercase and
grouped as `XXXX-XXXX-XXXX`.

Successful response:

```http
HTTP/1.1 200 OK
Content-Type: application/vnd.kys.pvp+zip
Content-Length: 345678
ETag: "<archive_sha256>"
X-Kys-Archive-Sha256: <archive_sha256>
X-Kys-Game-Version: 1.4.0
X-Kys-Format-Version: 1
Content-Disposition: attachment; filename="kys-opponent-7K3M-R9FD-W2HX.kyspvp"

<raw .kyspvp bytes>
```

The service streams the stored file and does not decompress it. Missing, expired, or
deleted codes return `404 Not Found`. A future metadata or browse endpoint may be added
without changing this response.

### Error format

JSON API failures use:

```json
{
  "error": {
    "code": "challenge_expired",
    "message": "Upload challenge expired",
    "retry_after_seconds": 0
  }
}
```

The client switches on the stable ASCII `code` and displays localized Traditional
Chinese text. The server `message` is for diagnostics and is not displayed directly as
trusted UI text.

Initial error codes:

| HTTP | Code | Meaning |
|---:|---|---|
| 400 | `malformed_request` | JSON, header, identifier, or number is malformed |
| 400 | `unsupported_protocol` | HTTP protocol version is unsupported |
| 400 | `unsupported_format` | Package format version is unsupported |
| 400 | `unsupported_game_version` | Server is not accepting this game version |
| 404 | `save_not_found` | Share code is absent or expired |
| 404 | `challenge_not_found` | Challenge identifier is unknown |
| 409 | `challenge_consumed` | Challenge is consumed without a recoverable receipt |
| 409 | `challenge_not_solved` | Archive upload was attempted before proof authorization |
| 410 | `challenge_expired` | Challenge lifetime ended |
| 410 | `upload_authorization_expired` | Solved challenge upload authorization ended |
| 413 | `upload_too_large` | Declared or received bytes exceed the limit |
| 422 | `invalid_proof` | Counter does not satisfy the challenge |
| 422 | `invalid_upload_token` | Upload token does not match the solved challenge |
| 422 | `archive_size_mismatch` | Received byte count differs from the challenge |
| 422 | `archive_hash_mismatch` | Received SHA-256 differs from the challenge |
| 429 | `rate_limited` | nginx or service request limit was exceeded |
| 503 | `storage_unavailable` | Database or filesystem is temporarily unavailable |

nginx-generated errors may not use this JSON envelope. The client must also handle HTTP
status and network failures generically.

## Identifier Rules

### Upload ID

- Generated only by the server.
- UUIDv7 or an equivalently collision-resistant 128-bit identifier.
- Used for database and filesystem identity.
- Never derived from a client filename.

### Share code

- Twelve random Crockford Base32 characters: 60 random bits.
- Canonical display groups: `XXXX-XXXX-XXXX`.
- The alphabet excludes `I`, `L`, `O`, and `U`.
- Generated from a cryptographically secure random source.
- Protected by a unique SQLite constraint; regenerate on collision.
- Treated as a locator, not an ownership credential.

The initial UI derives its label directly:

```text
對手存檔 · 7K3M-R9FD-W2HX
```

There is no player-controlled filename or display-name collision to resolve.

## Server Design

### Repository layout

```text
server/
└── kys-pvp-server/
    ├── Cargo.toml
    ├── Cargo.lock
    ├── rust-toolchain.toml
    ├── .cargo/
    │   └── config.toml
    ├── migrations/
    ├── src/
    │   ├── main.rs
    │   ├── api.rs
    │   ├── config.rs
    │   ├── database.rs
    │   ├── error.rs
    │   ├── identifiers.rs
    │   ├── pow.rs
    │   ├── storage.rs
    │   └── cleanup.rs
    ├── tests/
    ├── build-release.sh
    └── README.md
```

The Rust project is standalone rather than making the C++ repository root a Cargo
workspace. `Cargo.lock` is committed. `rust-toolchain.toml` pins the selected Rust
release once implementation begins and includes `rustfmt`, `clippy`, and the musl
target.

### Suggested Rust components

- `axum` for HTTP routing and extraction.
- `tokio` for the runtime and file streaming.
- `serde` and `serde_json` for protocol JSON.
- `rusqlite` with `bundled` SQLite.
- `sha2` for SHA-256.
- `rand` or `getrandom` for nonces and identifiers.
- `uuid` for UUID generation.
- `tracing` and `tracing-subscriber` for structured logs.
- Minimal `tower-http` middleware where it provides concrete value.

nginx terminates HTTPS, so the Rust service uses HTTP only and does not depend on
OpenSSL or a Rust TLS stack.

### Configuration

Configuration is loaded from one JSON file with optional environment overrides.
Required settings include:

```json
{
  "listen": "127.0.0.1:8787",
  "data_directory": "/var/lib/kys-pvp",
  "accepted_game_versions": ["1.4.0"],
  "maximum_upload_bytes": 0,
  "challenge_ttl_seconds": 300,
  "upload_authorization_ttl_seconds": 300,
  "completed_challenge_retention_seconds": 86400,
  "proof_of_work_bits": 0,
  "save_retention_days": 60,
  "maximum_failed_upload_attempts": 3
}
```

Zero values shown above are placeholders that must be replaced after save-size and
proof-of-work benchmarks. Production startup rejects unsafe placeholder values.

The service trusts forwarded client-address headers only because it listens on loopback
and receives traffic exclusively from the local nginx process.

### SQLite schema

Conceptual challenge table:

```text
upload_challenges
  challenge_id primary key
  nonce
  protocol_version
  format_version
  game_version
  archive_size_bytes
  archive_sha256
  difficulty_bits
  upload_token_sha256 nullable
  created_at
  expires_at
  upload_expires_at nullable
  completed_at nullable
  failed_attempts
  state
  upload_id nullable
```

Conceptual save table:

```text
saves
  upload_id primary key
  share_code unique
  protocol_version
  format_version
  game_version
  archive_size_bytes
  archive_sha256
  relative_file_path unique
  created_at
  expires_at
  state
```

SQLite runs in WAL mode on a local filesystem. Migrations are embedded into the binary
and execute on startup. The service is initially a single process, so no distributed
locking is required.

### File storage

```text
<data_directory>/
├── metadata.sqlite3
├── files/
│   └── <shard>/
│       └── <upload-id>.kyspvp
└── temporary/
    └── <challenge-id>.part
```

The shard is derived from the upload ID to avoid an indefinitely large single
directory. Client values never become path components.

The service writes a unique temporary file, verifies length and hash, synchronizes and
closes it, then performs an atomic rename on the same filesystem. Startup and scheduled
maintenance remove stale temporary files and reconcile orphaned files left by an
interrupted commit.

### Cleanup

A periodic task:

- Deletes expired pending challenges.
- Retains completed challenge receipts for the configured idempotency window, then
  deletes them without deleting the associated save.
- Deletes stale temporary files.
- Deletes expired save files and database rows.
- Retries or reports failed filesystem cleanup.
- Never follows symlinks from the managed data directory.

Retention is an operational storage control, not a promise that an upload remains
available for the entire period.

### Logging

Structured logs include:

- Request ID.
- Route and HTTP result.
- Upload or challenge ID when available.
- Received byte count.
- Proof and storage failure code.
- Cleanup counts and failures.

Logs must not contain archive bodies, checkpoint JSON, nonces plus solution counters,
or arbitrary client text. Share codes may be logged for operational lookup but are not
authentication secrets.

## Client Design

### Common C++ boundaries

Suggested components:

```text
ChessPvpPackage
  build checkpoint JSON into .kyspvp bytes
  inspect and extract .kyspvp bytes with strict limits

ChessPvpProofOfWork
  construct canonical proof input
  step through a bounded number of counters
  report progress and support cancellation

ChessPvpSharingClient
  create upload challenge
  upload archive
  download by share code

ChessPvpPublicationStore
  persist successful local publication receipts

ChessPvpOnlineController / UI
  coordinate package verification, networking, proof, receipts, and messages
```

Package creation, proof-of-work, protocol DTOs, receipt persistence, and UI flow are
shared. Only HTTP transport and platform persistence details differ.

### HTTP transport

Windows and Android share a `libcurl` transport. The existing raw Asio battle networking
is not reused: it is not an HTTP/TLS client and cannot provide the WASM path.

Required dependency changes:

- Add curl to the Windows vcpkg manifest with the Windows certificate-store backend.
- Add curl to the Android vcpkg manifest with an Android-compatible TLS backend.
- Do not add curl to the WASM manifest.
- Expose cancellation, status, response headers, byte limits, and upload/download
  progress through the common abstraction.

All callbacks that affect UI state return to the game/UI thread. Native network work
must not block the render thread.

### WASM transport

The WASM implementation uses browser `fetch` through the existing JavaScript bridge
style in `wasm/shell.html`.

- Use same-origin relative URLs under `/pvp-api/v1`.
- Transfer archive bodies as `Uint8Array`/`ArrayBuffer`, not text.
- Read response headers required for archive verification.
- Support abort through `AbortController`.
- Enforce download byte limits even when `Content-Length` is absent or dishonest.
- Do not rely on CORS as authentication.
- Keep the API on the game origin so no separate CORS configuration is required.

### Android setup

Add the normal install-time permission:

```xml
<uses-permission android:name="android.permission.INTERNET" />
```

No runtime permission prompt is required. `ACCESS_NETWORK_STATE` is optional and should
only be added if the UI needs explicit offline-state detection. HTTPS requires no
cleartext traffic exception. Downloading into app-private memory/storage requires no
external storage permission.

### Proof-of-work execution

Proof-of-work must not freeze browser or Android rendering. Implement it as a common
incremental solver that tests a bounded counter batch per `step()` call. The controller
advances it each frame or scheduled work slice, displays progress, and supports cancel.

Native platforms may later move the same solver onto a worker thread, but the shared
cooperative implementation is the correctness baseline and avoids requiring WASM
pthreads or a separate JavaScript algorithm.

The UI displays operation state rather than pretending that the total search space is
known:

```text
正在準備上傳證明...
已嘗試 2,400,000 次
[取消]
```

### Publication receipts

After a successful upload, persist:

```json
{
  "upload_id": "019f7465-a851-7fd5-b627-aeb13bd62222",
  "share_code": "7K3M-R9FD-W2HX",
  "game_version": "1.4.0",
  "format_version": 1,
  "archive_size_bytes": 345678,
  "archive_sha256": "64 lowercase hexadecimal characters",
  "created_at": "2026-07-18T12:01:00Z",
  "expires_at": "2026-09-16T12:01:00Z"
}
```

The receipt allows the player to copy the code again without remembering it. It does
not grant server ownership or deletion rights. Losing browser or app data loses the
local receipt but does not affect the published file.

WASM receipt writes must flush through the existing persistent filesystem sync path.

### UI flow

Extend the completed offline PvP screen with online actions while retaining manual file
import/export:

```text
[Publish My Save]
[Download by Share Code]
[Import Opponent File]
[Export My Save]
```

Publish states:

```text
Packaging
Verifying package
Requesting challenge
Solving proof-of-work
Uploading
Published
Failed / Cancelled
```

Download states:

```text
Downloading
Checking package
Verifying opponent save
Ready
Failed / Cancelled
```

Network and server errors must not replace or mutate the current opponent. A newly
downloaded opponent replaces the current opponent only after strict verification
succeeds.

## nginx Integration

The existing nginx instance remains responsible for the public deployment. Add routing
for the loopback Rust service:

```nginx
location /pvp-api/ {
    proxy_pass http://127.0.0.1:8787/;
    client_max_body_size <match operational upload limit>;

    proxy_set_header Host $host;
    proxy_set_header X-Real-IP $remote_addr;
    proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
    proxy_set_header X-Forwarded-Proto $scheme;
}
```

The final configuration should define separate rate-limit zones for challenge creation,
proof submission, upload, and download. The archive-upload location should disable nginx
request buffering where practical so the Rust service can reject a missing or invalid
upload authorization before nginx receives and buffers the complete request body. The
body-size limit remains enforced by both nginx and Rust.

The Rust service repeats security-critical byte, proof, and rate checks. nginx is an
outer operational limit, not the sole application validator.

## Rust and musl Development Setup

### WSL Ubuntu 24.04 prerequisites

```bash
sudo apt update
sudo apt install build-essential musl-tools

rustup target add x86_64-unknown-linux-musl
```

The Rust project records the musl linker explicitly:

```toml
# server/kys-pvp-server/.cargo/config.toml
[target.x86_64-unknown-linux-musl]
linker = "musl-gcc"
```

Native development remains fast and convenient:

```bash
cd server/kys-pvp-server
cargo test --locked
cargo run
```

The final distributable binary is built separately:

```bash
cargo build \
  --locked \
  --release \
  --target x86_64-unknown-linux-musl
```

Output:

```text
server/kys-pvp-server/target/x86_64-unknown-linux-musl/release/kys-pvp-server
```

Verification:

```bash
file target/x86_64-unknown-linux-musl/release/kys-pvp-server
ldd target/x86_64-unknown-linux-musl/release/kys-pvp-server
```

The executable must report as statically linked or as not a dynamic executable. Release
builds must not use `-C target-cpu=native`, because the WSL development CPU may support
instructions unavailable on the deployment host.

If the deployment architecture is ARM64, add and build
`aarch64-unknown-linux-musl` with an appropriate cross-linker instead. The initial target
is `x86_64-unknown-linux-musl` after confirming the existing container reports
`x86_64`/`amd64`.

### Rust quality checks

```bash
cargo fmt --check
cargo clippy --locked --all-targets -- -D warnings
cargo test --locked
cargo build --locked --release --target x86_64-unknown-linux-musl
```

`build-release.sh` should wrap these checks and the static build for local release use.
Repository-wide build automation may invoke it when WSL or a Linux runner is available;
the existing C++ build path remains responsible for the game targets.

## High-level Distribution Note

Detailed production deployment is deferred. The intended shape is:

1. Build the static musl binary in WSL or CI.
2. Produce and verify a SHA-256 checksum.
3. Copy the versioned executable into persistent storage visible inside the existing
   nginx container.
4. Keep configuration and `/var/lib/kys-pvp` data persistent across container
   recreation.
5. Register the service with the process supervisor already used by that container, or
   extend its startup lifecycle so the service starts and restarts reliably.
6. Point nginx `/pvp-api/` at `127.0.0.1:8787`.
7. Check `/pvp-api/healthz` after deployment.
8. Retain the previous executable for rollback.

Copying a binary into only the writable container layer is acceptable for a temporary
test but is not the final persistence mechanism.

## Testing

### Common client tests

- Package writer creates exactly the required entries.
- Package reader rejects extra, missing, duplicate, encrypted, oversized, and unsupported
  entries.
- Manifest/checkpoint hash and size mismatches are rejected.
- A packaged valid checkpoint passes strict PvP verification after re-extraction.
- A packaged invalid checkpoint is rejected before upload.
- Archive SHA mismatch is rejected before decompression.
- Share-code normalization accepts case and hyphens but rejects other characters.
- Proof input construction matches the Rust test vectors.
- Incremental proof solving finds known solutions and supports cancellation.
- Publication receipts serialize, restore, and persist on WASM.

### Server unit tests

- Challenge generation uses unique cryptographic nonces.
- Challenge request validation covers every field and limit.
- Proof verification matches the C++ test vectors.
- Challenges expire and cannot bind to different hashes or sizes.
- Valid proof submission returns one stable short-lived upload authorization.
- Invalid or expired upload tokens are rejected before permanent storage.
- Successful upload retries return the original receipt.
- Invalid proof is rejected before permanent storage.
- Size and hash mismatches never create active save rows.
- Upload and share-code collisions retry safely under unique constraints.
- Share-code normalization is case-insensitive and hyphen-insensitive.
- Download headers match stored metadata.
- Cleanup removes expired rows, files, and stale temporary uploads.
- Storage recovery handles interrupted commits and orphaned files.

### Integration tests

- Request challenge, solve and submit a known low-difficulty proof, upload with the
  returned authorization, and download identical bytes.
- Lose the first upload response and recover the same receipt by retrying.
- Downloaded archive passes client package and PvP verification.
- nginx body limits and service body limits agree.
- Rate-limited responses are handled by each platform client.
- Windows uses trusted HTTPS without a permission prompt.
- Android networking works with `INTERNET` and no runtime permission.
- WASM uses same-origin binary fetch and cancellation.

## Implementation Order

1. Finalize measured package-size limits, initial proof target, challenge lifetime, and
   retention defaults.
2. Implement common C++ package writing/reading and its tests.
3. Implement common proof-of-work construction, incremental solving, and shared test
   vectors.
4. Scaffold `server/kys-pvp-server` with pinned Rust, musl configuration, health check,
   config loading, logging, and migrations.
5. Implement Rust proof verification and protocol DTO tests using the shared vectors.
6. Implement SQLite challenge/save state and filesystem storage.
7. Implement challenge, proof submission, authorized upload, idempotency, download, and
   cleanup endpoints.
8. Add Windows/Android curl transport and Android `INTERNET` permission.
9. Add WASM binary fetch/abort bridge.
10. Implement publication receipts and platform persistence.
11. Add publish/download UI states to the existing offline PvP screen.
12. Run C++ unit tests and the required project builds for changed platforms.
13. Run Rust formatting, lint, native tests, and static musl release build.
14. Perform a local nginx end-to-end test before production deployment planning.

## Open Operational Values

The architecture and protocol do not depend on these exact values, but implementation
must select them from measurements before enabling production uploads:

- Maximum compressed archive bytes.
- Maximum combined uncompressed package bytes.
- Proof-of-work difficulty or target device solve time.
- Challenge lifetime.
- Challenge creation and upload rate limits.
- Save retention period.
- Maximum failed attempts per challenge.
- Exact accepted game-version rollout policy during upgrades.
- Existing-container supervisor and persistent mount paths.
