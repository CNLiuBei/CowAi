-- Migration: Add license keys tables
-- Run this after the initial schema

-- 卡密表
CREATE TABLE IF NOT EXISTS license_keys (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    key_code TEXT UNIQUE NOT NULL,
    key_type TEXT NOT NULL DEFAULT 'time',  -- 'time' (时间卡), 'permanent' (永久卡)
    duration_days INTEGER,                   -- 有效天数 (time 类型)
    max_devices INTEGER DEFAULT 1,           -- 最大设备数
    created_by TEXT NOT NULL,                -- 创建者 tg_user_id
    created_at INTEGER NOT NULL,
    activated_by TEXT,                       -- 激活者 (device_id 或 tg_user_id)
    activated_at INTEGER,
    expires_at INTEGER,                      -- 过期时间 (激活后计算)
    status TEXT DEFAULT 'unused',            -- 'unused', 'activated', 'expired', 'revoked'
    note TEXT                                -- 备注
);

-- 卡密设备绑定表
CREATE TABLE IF NOT EXISTS key_devices (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    key_id INTEGER NOT NULL,
    device_id TEXT NOT NULL,
    bound_at INTEGER NOT NULL,
    last_seen INTEGER NOT NULL,
    is_active INTEGER DEFAULT 1,
    UNIQUE(key_id, device_id),
    FOREIGN KEY (key_id) REFERENCES license_keys(id)
);

-- 索引
CREATE INDEX IF NOT EXISTS idx_license_keys_key_code ON license_keys(key_code);
CREATE INDEX IF NOT EXISTS idx_license_keys_status ON license_keys(status);
CREATE INDEX IF NOT EXISTS idx_license_keys_activated_by ON license_keys(activated_by);
CREATE INDEX IF NOT EXISTS idx_key_devices_key_id ON key_devices(key_id);
CREATE INDEX IF NOT EXISTS idx_key_devices_device_id ON key_devices(device_id);
