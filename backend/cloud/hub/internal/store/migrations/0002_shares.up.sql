-- yuleDKCS Hub 0002: 密钥分享表 + keys 权限位
-- 由 golang-migrate 执行 (iofs source)

-- ─── 密钥分享记录 ──────────────────────────────────────────
CREATE TABLE IF NOT EXISTS shares (
    share_id      TEXT PRIMARY KEY,
    key_id        TEXT NOT NULL,          -- 被分享的钥匙 id
    from_user_id  TEXT NOT NULL,          -- 分享发起方 (车主)
    to_user_id    TEXT NOT NULL DEFAULT '',  -- 指定接收方 (空=按分享码接受)
    to_vendor     TEXT NOT NULL DEFAULT '',
    share_code    TEXT NOT NULL,          -- 分享码 (6位数字, 唯一)
    status        TEXT NOT NULL DEFAULT 'pending',  -- pending / accepted / cancelled
    access_bits   INTEGER NOT NULL DEFAULT 0,       -- 分享授予的权限位掩码
    valid_from    BIGINT NOT NULL DEFAULT 0,
    valid_until   BIGINT NOT NULL DEFAULT 0,
    max_uses      INTEGER NOT NULL DEFAULT 0,
    friend_key_id TEXT NOT NULL DEFAULT '',         -- 接受后生成的 friend key id
    created_at    BIGINT NOT NULL,                  -- unix millis
    accepted_at   BIGINT NOT NULL DEFAULT 0,
    cancelled_at  BIGINT NOT NULL DEFAULT 0
);

CREATE UNIQUE INDEX IF NOT EXISTS idx_shares_code ON shares (share_code);
CREATE INDEX IF NOT EXISTS idx_shares_key ON shares (key_id);

-- ─── keys 表增加权限位列 ──────────────────────────────────
ALTER TABLE keys ADD COLUMN IF NOT EXISTS access_bits INTEGER NOT NULL DEFAULT 0;
