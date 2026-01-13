// 环境变量和绑定类型
export interface Env {
  // D1 数据库
  DB: D1Database;
  // KV 存储
  KV: KVNamespace;
  // 静态资源
  ASSETS: Fetcher;
  // 环境变量
  BOT_USERNAME: string;
  BOT_TOKEN: string;           // Secret
  JWT_SECRET: string;          // Secret
  ADMIN_TG_IDS: string;        // Secret: 逗号分隔的管理员 TG ID
  BOT_SECRET: string;          // Secret: Bot API 调用密钥
  SESSION_TIMEOUT_SEC: string;
  TOKEN_EXPIRE_DAYS: string;
  DEFAULT_DEVICE_LIMIT: string;
  RATE_LIMIT_REQUESTS: string;
  RATE_LIMIT_WINDOW_SEC: string;
  // 支付宝配置
  ALIPAY_APP_ID: string;       // Secret: 支付宝应用ID
  ALIPAY_PRIVATE_KEY: string;  // Secret: 应用私钥
  ALIPAY_PUBLIC_KEY: string;   // Secret: 支付宝公钥
  ALIPAY_NOTIFY_URL?: string;  // 支付回调URL
}

// 会话状态
export type SessionStatus = 'pending' | 'approved' | 'rejected' | 'expired';

// 数据库模型
export interface User {
  tg_user_id: string;
  username: string | null;
  first_name: string | null;
  is_banned: number;
  device_limit: number;
  created_at: number;
  updated_at: number;
}

export interface Device {
  id: number;
  device_id: string;
  tg_user_id: string;
  device_info: string | null;
  bound_at: number;
  last_seen: number;
  is_active: number;
}

export interface Session {
  session_id: string;
  device_id: string;
  tg_user_id: string | null;
  status: SessionStatus;
  created_at: number;
  expires_at: number;
  approved_at: number | null;
}

export interface AuditLog {
  id: number;
  action: string;
  tg_user_id: string | null;
  device_id: string | null;
  ip_address: string | null;
  details: string | null;
  created_at: number;
}

// JWT Payload
export interface JWTPayload {
  sub: string;      // tg_user_id
  did: string;      // device_id
  iat: number;      // issued at
  exp: number;      // expires at
  jti: string;      // unique token id
}

// API 响应类型
export interface ApiResponse<T = unknown> {
  success: boolean;
  data?: T;
  error?: {
    code: string;
    message: string;
  };
}

// 请求登录响应
export interface RequestLoginResponse {
  session_id: string;
  telegram_url: string;
  expires_at: number;
}

// 检查登录响应
export interface CheckLoginResponse {
  status: SessionStatus;
  access_token?: string;
  expires_at?: number;
  user?: {
    tg_user_id: string;
    username: string | null;
  };
}

// 验证 Token 响应
export interface VerifyTokenResponse {
  valid: boolean;
  user?: {
    tg_user_id: string;
    username: string | null;
  };
  expires_at?: number;
}

// Telegram Update 类型
export interface TelegramUpdate {
  update_id: number;
  message?: TelegramMessage;
  callback_query?: TelegramCallbackQuery;
}

export interface TelegramMessage {
  message_id: number;
  from?: TelegramUser;
  chat: TelegramChat;
  date: number;
  text?: string;
}

export interface TelegramCallbackQuery {
  id: string;
  from: TelegramUser;
  message?: TelegramMessage;
  data?: string;
}

export interface TelegramUser {
  id: number;
  is_bot: boolean;
  first_name: string;
  last_name?: string;
  username?: string;
}

export interface TelegramChat {
  id: number;
  type: string;
  title?: string;
  username?: string;
  first_name?: string;
  last_name?: string;
}

// 错误码
export const ErrorCodes = {
  SESSION_NOT_FOUND: 'SESSION_NOT_FOUND',
  SESSION_EXPIRED: 'SESSION_EXPIRED',
  TOKEN_INVALID: 'TOKEN_INVALID',
  TOKEN_EXPIRED: 'TOKEN_EXPIRED',
  DEVICE_MISMATCH: 'DEVICE_MISMATCH',
  USER_BANNED: 'USER_BANNED',
  DEVICE_LIMIT_EXCEEDED: 'DEVICE_LIMIT_EXCEEDED',
  RATE_LIMITED: 'RATE_LIMITED',
  UNAUTHORIZED: 'UNAUTHORIZED',
  BAD_REQUEST: 'BAD_REQUEST',
  INTERNAL_ERROR: 'INTERNAL_ERROR',
  FORBIDDEN: 'FORBIDDEN',
  NOT_FOUND: 'NOT_FOUND',
  KEY_INVALID: 'KEY_INVALID',
  KEY_EXPIRED: 'KEY_EXPIRED',
  KEY_REVOKED: 'KEY_REVOKED',
} as const;

export type ErrorCode = typeof ErrorCodes[keyof typeof ErrorCodes];
