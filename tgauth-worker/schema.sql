-- TGAuth D1 Database Schema
-- 用于 Cloudflare D1 数据库

-- 用户表
CREATE TABLE IF NOT EXISTS users (
    tg_user_id TEXT PRIMARY KEY,
    username TEXT,
    first_name TEXT,
    is_banned INTEGER DEFAULT 0,
    device_limit INTEGER DEFAULT 1,
    created_at INTEGER NOT NULL,
    updated_at INTEGER NOT NULL
);

-- 设备表
CREATE TABLE IF NOT EXISTS devices (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    device_id TEXT NOT NULL,
    tg_user_id TEXT NOT NULL,
    device_info TEXT,
    bound_at INTEGER NOT NULL,
    last_seen INTEGER NOT NULL,
    is_active INTEGER DEFAULT 1,
    UNIQUE(device_id, tg_user_id),
    FOREIGN KEY (tg_user_id) REFERENCES users(tg_user_id)
);

-- 会话表
CREATE TABLE IF NOT EXISTS sessions (
    session_id TEXT PRIMARY KEY,
    device_id TEXT NOT NULL,
    tg_user_id TEXT,
    status TEXT NOT NULL DEFAULT 'pending',
    created_at INTEGER NOT NULL,
    expires_at INTEGER NOT NULL,
    approved_at INTEGER
);

-- 审计日志表
CREATE TABLE IF NOT EXISTS audit_logs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    action TEXT NOT NULL,
    tg_user_id TEXT,
    device_id TEXT,
    ip_address TEXT,
    details TEXT,
    created_at INTEGER NOT NULL
);

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
CREATE INDEX IF NOT EXISTS idx_devices_tg_user_id ON devices(tg_user_id);
CREATE INDEX IF NOT EXISTS idx_devices_device_id ON devices(device_id);
CREATE INDEX IF NOT EXISTS idx_sessions_status ON sessions(status);
CREATE INDEX IF NOT EXISTS idx_sessions_expires ON sessions(expires_at);
CREATE INDEX IF NOT EXISTS idx_sessions_device_id ON sessions(device_id);
CREATE INDEX IF NOT EXISTS idx_audit_logs_tg_user_id ON audit_logs(tg_user_id);
CREATE INDEX IF NOT EXISTS idx_audit_logs_created_at ON audit_logs(created_at);
CREATE INDEX IF NOT EXISTS idx_audit_logs_action ON audit_logs(action);
CREATE INDEX IF NOT EXISTS idx_users_is_banned ON users(is_banned);

-- 订单表
CREATE TABLE IF NOT EXISTS orders (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    order_no TEXT UNIQUE NOT NULL,           -- 商户订单号
    trade_no TEXT,                           -- 支付宝交易号
    product_id TEXT NOT NULL,                -- 商品ID
    product_name TEXT NOT NULL,              -- 商品名称
    amount REAL NOT NULL,                    -- 金额(元)
    status TEXT DEFAULT 'pending',           -- 'pending', 'paid', 'delivered', 'expired', 'refunded'
    buyer_id TEXT,                           -- 购买者标识 (可选)
    key_code TEXT,                           -- 发放的卡密
    qr_code TEXT,                            -- 支付二维码链接
    created_at INTEGER NOT NULL,
    paid_at INTEGER,
    delivered_at INTEGER,
    expires_at INTEGER NOT NULL              -- 订单过期时间
);

-- 商品表
CREATE TABLE IF NOT EXISTS products (
    id TEXT PRIMARY KEY,                     -- 商品ID
    name TEXT NOT NULL,                      -- 商品名称
    description TEXT,                        -- 描述
    price REAL NOT NULL,                     -- 价格(元)
    key_type TEXT NOT NULL DEFAULT 'time',   -- 卡密类型
    duration_days INTEGER,                   -- 有效天数
    max_devices INTEGER DEFAULT 1,           -- 最大设备数
    is_active INTEGER DEFAULT 1,             -- 是否上架
    sort_order INTEGER DEFAULT 0,            -- 排序
    created_at INTEGER NOT NULL
);

-- 订单索引
CREATE INDEX IF NOT EXISTS idx_orders_order_no ON orders(order_no);
CREATE INDEX IF NOT EXISTS idx_orders_trade_no ON orders(trade_no);
CREATE INDEX IF NOT EXISTS idx_orders_status ON orders(status);
CREATE INDEX IF NOT EXISTS idx_orders_created_at ON orders(created_at);
CREATE INDEX IF NOT EXISTS idx_products_is_active ON products(is_active);
