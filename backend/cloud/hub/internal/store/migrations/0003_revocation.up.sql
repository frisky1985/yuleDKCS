-- 设备吊销列表 (CRL) 持久化。
-- token 是 16 字节撤销令牌 (SHA-256(domain || id)[0:15])，见 sec_revocation.h。
CREATE TABLE IF NOT EXISTS revocation_state (
    id    BOOLEAN PRIMARY KEY DEFAULT TRUE CHECK (id),
    epoch BIGINT NOT NULL DEFAULT 0
);

INSERT INTO revocation_state (id) VALUES (TRUE)
ON CONFLICT (id) DO NOTHING;

CREATE TABLE IF NOT EXISTS revocation_entries (
    token      BYTEA PRIMARY KEY,
    entry_type SMALLINT NOT NULL CHECK (entry_type BETWEEN 1 AND 3),
    created_at TIMESTAMPTZ NOT NULL DEFAULT now()
);
