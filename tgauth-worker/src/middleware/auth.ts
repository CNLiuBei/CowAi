import { Context, Next } from 'hono';
import { Env, ApiResponse, ErrorCodes } from '../types';
import { JWTService } from '../services/jwt';
import { DeviceService } from '../services/device';

// Token 认证中间件
export async function authMiddleware(c: Context<{ Bindings: Env }>, next: Next) {
  const authHeader = c.req.header('Authorization');
  const deviceId = c.req.header('X-Device-ID');

  // 检查必要的 headers
  if (!authHeader) {
    return c.json<ApiResponse>({
      success: false,
      error: {
        code: ErrorCodes.BAD_REQUEST,
        message: 'Authorization header is required',
      },
    }, 400);
  }

  if (!deviceId) {
    return c.json<ApiResponse>({
      success: false,
      error: {
        code: ErrorCodes.BAD_REQUEST,
        message: 'X-Device-ID header is required',
      },
    }, 400);
  }

  // 提取 token
  if (!authHeader.startsWith('Bearer ')) {
    return c.json<ApiResponse>({
      success: false,
      error: {
        code: ErrorCodes.TOKEN_INVALID,
        message: 'Invalid authorization format',
      },
    }, 401);
  }

  const token = authHeader.substring(7);
  const jwtService = new JWTService(c.env);
  const deviceService = new DeviceService(c.env);

  // 验证 token
  const payload = await jwtService.verifyToken(token);
  if (!payload) {
    return c.json<ApiResponse>({
      success: false,
      error: {
        code: ErrorCodes.TOKEN_INVALID,
        message: 'Invalid or expired token',
      },
    }, 401);
  }

  // 检查 device_id 是否匹配
  if (payload.did !== deviceId) {
    return c.json<ApiResponse>({
      success: false,
      error: {
        code: ErrorCodes.DEVICE_MISMATCH,
        message: 'Device ID does not match token',
      },
    }, 403);
  }

  // 卡密登录 (tg_user_id 格式为 "key:123") - 跳过普通用户检查，由 verify_token 处理
  if (payload.sub.startsWith('key:')) {
    c.set('user' as never, {
      tg_user_id: payload.sub,
      device_id: payload.did,
      exp: payload.exp,
    });
    await next();
    return;
  }

  // 普通 Telegram 登录 - 检查用户是否被封禁
  const banned = await c.env.KV.get(`banned:${payload.sub}`);
  if (banned) {
    return c.json<ApiResponse>({
      success: false,
      error: {
        code: ErrorCodes.USER_BANNED,
        message: 'User is banned',
      },
    }, 403);
  }

  // 检查设备是否被解绑
  const unbound = await deviceService.isDeviceUnbound(deviceId);
  if (unbound) {
    return c.json<ApiResponse>({
      success: false,
      error: {
        code: ErrorCodes.TOKEN_INVALID,
        message: 'Device has been unbound',
      },
    }, 401);
  }

  // 更新设备最后活跃时间
  await deviceService.updateLastSeen(payload.sub, deviceId);

  // 将用户信息存储到上下文
  c.set('user' as never, {
    tg_user_id: payload.sub,
    device_id: payload.did,
    exp: payload.exp,
  });

  await next();
}

// 管理员认证中间件
export async function adminAuthMiddleware(c: Context<{ Bindings: Env }>, next: Next) {
  const adminSecret = c.req.header('X-Admin-Secret');
  const botSecret = c.req.header('X-Bot-Secret');
  
  // 方式 1: 使用 Admin Secret 或 Bot Secret
  if ((adminSecret && adminSecret === c.env.BOT_SECRET) || 
      (botSecret && botSecret === c.env.BOT_SECRET)) {
    // 从请求中获取管理员 ID (如果有)
    const adminId = c.req.header('X-Admin-ID') || 'api';
    c.set('adminId' as never, adminId);
    await next();
    return;
  }

  // 方式 2: 使用 Bearer Token + 管理员验证
  const authHeader = c.req.header('Authorization');
  if (authHeader?.startsWith('Bearer ')) {
    const token = authHeader.substring(7);
    const jwtService = new JWTService(c.env);
    const payload = await jwtService.verifyToken(token);

    if (payload) {
      // 检查是否是管理员
      const adminIds = (c.env.ADMIN_TG_IDS || '').split(',').map(id => id.trim());
      if (adminIds.includes(payload.sub)) {
        c.set('adminId' as never, payload.sub);
        await next();
        return;
      }
    }
  }

  return c.json<ApiResponse>({
    success: false,
    error: {
      code: ErrorCodes.FORBIDDEN,
      message: 'Admin access required',
    },
  }, 403);
}
