-- yuleDKCS Hub 初始 schema
-- 由 golang-migrate 执行 (iofs source)

-- ─── 数字钥匙元数据 ────────────────────────────────────────
CREATE TABLE IF NOT EXISTS keys (
    key_id        TEXT PRIMARY KEY,
    owner_user_id TEXT NOT NULL,
    vehicle_id    TEXT NOT NULL,
    vendor        TEXT NOT NULL DEFAULT '',
    status        TEXT NOT NULL DEFAULT 'active',  -- active / suspended / revoked / pending
    created_at    BIGINT NOT NULL                  -- unix millis
);

CREATE INDEX IF NOT EXISTS idx_keys_owner ON keys (owner_user_id);
CREATE INDEX IF NOT EXISTS idx_keys_vehicle ON keys (vehicle_id);

-- ─── CCC Mailbox ───────────────────────────────────────────
CREATE TABLE IF NOT EXISTS mailboxes (
    mailbox_id          TEXT PRIMARY KEY,
    status              INTEGER NOT NULL,          -- MailboxStatus enum
    sender_device_id    TEXT NOT NULL DEFAULT '',
    sender_vendor       TEXT NOT NULL DEFAULT '',
    notification_token  TEXT NOT NULL DEFAULT '',
    sender_token        TEXT NOT NULL DEFAULT '',
    receiver_token      TEXT NOT NULL DEFAULT '',
    display_info        BYTEA,
    payload             BYTEA,
    sharing_data_type   INTEGER NOT NULL DEFAULT 0,
    sharing_url         TEXT NOT NULL DEFAULT '',
    receiver_device_id  TEXT NOT NULL DEFAULT '',
    receiver_vendor     TEXT NOT NULL DEFAULT '',
    created_at          TIMESTAMPTZ NOT NULL,
    expires_at          TIMESTAMPTZ NOT NULL,
    updated_at          TIMESTAMPTZ NOT NULL,
    version             BIGINT NOT NULL DEFAULT 1,
    update_count        INTEGER NOT NULL DEFAULT 0,
    max_updates         INTEGER NOT NULL DEFAULT 10,
    device_attestation  BYTEA
);

CREATE INDEX IF NOT EXISTS idx_mailboxes_expires ON mailboxes (expires_at);
CREATE INDEX IF NOT EXISTS idx_mailboxes_status ON mailboxes (status);
