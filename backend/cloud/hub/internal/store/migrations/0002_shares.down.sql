-- yuleDKCS Hub 0002 回滚: 删除密钥分享表 + keys 权限位列

DROP TABLE IF EXISTS shares;

ALTER TABLE keys DROP COLUMN IF EXISTS access_bits;
