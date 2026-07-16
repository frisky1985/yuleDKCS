# Kafka Message Queue Integration Report

## Task
GO-P0-02 (降级 P1) — Kafka producer integration for key share/revoke events.

## What Was Integrated

### 1. `internal/mq/types.go` — New file
- **`KeyEvent` struct**: Typed payload for digital key lifecycle events.
- **`KafkaProducer.PublishKeyEvent()`**: Convenience method that wraps `Publish()` with the `KeyEvent` format.
- **Constants**: `KeyEventShared`, `KeyEventRevoked`, `KeyEventActivated`, `KeyEventUpdated`.

### 2. `internal/service/interfaces.go` — New `EventBus` interface
- `EventBus.PublishKeyEvent(ctx, eventType, keyID, ownerID, targetID) error`
- Keeps the service layer free of `mq` imports (dependency inversion).

### 3. `internal/service/key_service.go` — Event emission points
- **`KeyService.eventBus`**: Optional field; nil = no-op.
- **`KeyService.WithEventBus()`**: Builder-pattern injection (follows `WithIdempotency` pattern).
- **`KeyService.emitKeyEvent()`**: Non-blocking helper using goroutine + `select` with 500ms timeout. Logs on failure but never blocks business flow.
- **Emission points** (post-success):
  - `ShareKey()` → `"key_shared"` (owner = original user, target = recipient)
  - `RevokeKey()` → `"key_revoked"` (owner = key owner)
  - `ActivateKey()` → `"key_activated"` (owner = key owner)

### 4. `cmd/dkcs/main.go` — Wiring
- **`kafkaEventBusAdapter`**: Adapts `*mq.KafkaProducer` → `service.EventBus`.
- **Injection**: `keyService.WithEventBus(adapter)` if Kafka producer initialised successfully.
- **Graceful degrade**: If Kafka unavailable, `kafkaProducer` is nil → adapter not wired → no events emitted → business flow continues uninterrupted.

### 5. `internal/mq/kafka_test.go` — Mock fix
- Added `AddMessageToTxnWithGroupMetadata` and `AddOffsetsToTxnWithGroupMetadata` to `mockSyncProducer` for sarama v1.50.3 compatibility.

## Event Message Format

```go
// KeyEvent — structured payload sent as Kafka Message.Data
type KeyEvent struct {
    EventType string `json:"event_type"` // key_shared | key_revoked | key_activated
    KeyID     string `json:"key_id"`
    OwnerID   string `json:"owner_id"`
    TargetID  string `json:"target_id,omitempty"` // 接收者 (分享时)
    Timestamp int64  `json:"timestamp"`
}
```

Wrapped in the existing `mq.Message` envelope:

```json
{
  "key": "{keyID}",
  "type": "key_shared|key_revoked|key_activated",
  "timestamp": 1712345678901,
  "data": {
    "event_type": "key_shared",
    "key_id": "{keyID}",
    "owner_id": "{ownerID}",
    "target_id": "{targetID}",
    "timestamp": 1712345678901
  }
}
```

Topic: `dkcs.key.events` (from `mq.TopicKeyEvents` / config `KAFKA_TOPIC_KEY_EVENTS`)

## Non-Blocking Strategy

```go
func (s *KeyService) emitKeyEvent(ctx, eventType, keyID, ownerID, targetID) {
    if s.eventBus == nil { return }                  // Kafka not configured
    go func() { done <- s.eventBus.PublishKeyEvent() }()
    select {
    case err := <-done:    // log if err
    case <-time.After(500ms): log warn (timeout)      // fire-and-forget
    }
}
```

- **Fire-and-forget**: goroutine + 500ms timeout channel.
- **Failures logged only**: Business flow never blocks on Kafka.
- **Nil bus**: Zero overhead when Kafka is not configured.

## Verification

```bash
cd ~/yuleDKCS/backend/dkcs && go build ./...    # OK (no errors)
cd ~/yuleDKCS/backend/dkcs && go vet ./...      # OK (no warnings)
cd ~/yuleDKCS/backend/dkcs && go test ./...     # All 12 packages PASS
```

```
ok  github.com/frisky1985/yuleDKCS/backend/dkcs/internal/mq       4.717s
ok  github.com/frisky1985/yuleDKCS/backend/dkcs/internal/service  3.384s
ok  github.com/frisky1985/yuleDKCS/backend/dkcs/cmd/dkcs          [no test files]
... (all 12 packages pass)
```

## Files Changed

| File | Change |
|------|--------|
| `internal/mq/types.go` | **NEW** — `KeyEvent`, `PublishKeyEvent()`, constants |
| `internal/service/interfaces.go` | **EDIT** — Added `EventBus` interface |
| `internal/service/key_service.go` | **EDIT** — `eventBus` field, `WithEventBus()`, `emitKeyEvent()`, emission in 3 handlers |
| `cmd/dkcs/main.go` | **EDIT** — `kafkaEventBusAdapter`, wiring keyService.WithEventBus() |
| `internal/mq/kafka_test.go` | **EDIT** — Added 2 missing mock methods for sarama v1.50.3 |

## Subsequent Suggestions

### Consumer-Side Integration (next step)

1. **Add consumer handler in a new file**, e.g. `internal/eventhandler/key_event_handler.go`:
   - Implement `mq.MessageHandler` interface
   - On `key_shared` → trigger notification (push/email)
   - On `key_revoked` → trigger access revocation on TCU
   - On `key_activated` → log activation for audit

2. **Wire consumer in `cmd/dkcs/main.go`**:
   - Create `KafkaConsumer` with group ID `"dkcs-key-events"`
   - Start in a goroutine
   - Handle graceful shutdown via context cancellation

3. **Consider dead-letter queue**:
   - `cfg.Kafka.Topics.DLQ` already configured (`dkcs.dlq`)
   - Route unprocessable events there with original payload + error

4. **Observability**:
   - Track `dkcs.event.published` counter in telemetry
   - Track `dkcs.event.publish.error` and `dkcs.event.publish.timeout`
