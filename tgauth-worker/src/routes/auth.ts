import { Hono } from 'hono';
import { Env, ApiResponse, RequestLoginResponse, CheckLoginResponse, VerifyTokenResponse, ErrorCodes } from '../types';
import { SessionService } from '../services/session';
import { JWTService } from '../services/jwt';
import { DeviceService } from '../services/device';
import { AuditService } from '../services/audit';
import { authMiddleware } from '../middleware/auth';

export const authRoutes = new Hono<{ Bindings: Env }>();

// POST /api/request_login - 请求登录
authRoutes.post('/request_login', async (c) => {
  const body = await c.req.json<{ device_id: string }>().catch(() => null);
  
  if (!body?.device_id) {
    return c.json<ApiResponse>({
      success: false,
      error: {
        code: ErrorCodes.BAD_REQUEST,
        message: 'device_id is required',
      },
    }, 400);
  }

  const sessionService = new SessionService(c.env);
  const auditService = new AuditService(c.env);
  
  try {
    // 创建会话
    const session = await sessionService.createSession(body.device_id);
    
    // 生成 Telegram 登录 URL
    const telegramUrl = `https://t.me/${c.env.BOT_USERNAME}?start=LOGIN_${session.session_id}`;
    
    // 记录审计日志
    await auditService.log({
      action: 'request_login',
      device_id: body.device_id,
      ip_address: c.req.header('CF-Connecting-IP') || null,
      details: JSON.stringify({ session_id: session.session_id }),
    });

    return c.json<ApiResponse<RequestLoginResponse>>({
      success: true,
      data: {
        session_id: session.session_id,
        telegram_url: telegramUrl,
        expires_at: session.expires_at,
      },
    });
  } catch (error) {
    console.error('Error creating session:', error);
    return c.json<ApiResponse>({
      success: false,
      error: {
        code: ErrorCodes.INTERNAL_ERROR,
        message: 'Failed to create login session',
      },
    }, 500);
  }
});

// GET /api/check_login - 检查登录状态
authRoutes.get('/check_login', async (c) => {
  const sessionId = c.req.query('session_id');
  
  if (!sessionId) {
    return c.json<ApiResponse>({
      success: false,
      error: {
        code: ErrorCodes.BAD_REQUEST,
        message: 'session_id is required',
      },
    }, 400);
  }

  const sessionService = new SessionService(c.env);
  const jwtService = new JWTService(c.env);
  
  try {
    const session = await sessionService.getSession(sessionId);
    
    if (!session) {
      return c.json<ApiResponse>({
        success: false,
        error: {
          code: ErrorCodes.SESSION_NOT_FOUND,
          message: 'Session not found',
        },
      }, 404);
    }

    // 检查是否过期
    if (session.expires_at < Date.now() / 1000) {
      return c.json<ApiResponse<CheckLoginResponse>>({
        success: true,
        data: {
          status: 'expired',
        },
      });
    }

    // 返回当前状态
    if (session.status === 'pending') {
      return c.json<ApiResponse<CheckLoginResponse>>({
        success: true,
        data: {
          status: 'pending',
        },
      });
    }

    if (session.status === 'rejected') {
      return c.json<ApiResponse<CheckLoginResponse>>({
        success: true,
        data: {
          status: 'rejected',
        },
      });
    }

    if (session.status === 'approved' && session.tg_user_id) {
      // 生成 JWT Token
      const token = await jwtService.generateToken({
        tg_user_id: session.tg_user_id,
        device_id: session.device_id,
      });

      // 获取用户信息
      const user = await c.env.DB.prepare(
        'SELECT tg_user_id, username FROM users WHERE tg_user_id = ?'
      ).bind(session.tg_user_id).first<{ tg_user_id: string; username: string | null }>();

      return c.json<ApiResponse<CheckLoginResponse>>({
        success: true,
        data: {
          status: 'approved',
          access_token: token.token,
          expires_at: token.expires_at,
          user: user ? {
            tg_user_id: user.tg_user_id,
            username: user.username,
          } : undefined,
        },
      });
    }

    return c.json<ApiResponse<CheckLoginResponse>>({
      success: true,
      data: {
        status: session.status,
      },
    });
  } catch (error) {
    console.error('Error checking login:', error);
    return c.json<ApiResponse>({
      success: false,
      error: {
        code: ErrorCodes.INTERNAL_ERROR,
        message: 'Failed to check login status',
      },
    }, 500);
  }
});

// POST /api/verify_token - 验证 Token
authRoutes.post('/verify_token', authMiddleware, async (c) => {
  // authMiddleware 已经验证了 token，这里直接返回成功
  const user = c.get('user' as never) as { tg_user_id: string; device_id: string; exp: number } | undefined;
  
  if (!user) {
    return c.json<ApiResponse>({
      success: false,
      error: {
        code: ErrorCodes.TOKEN_INVALID,
        message: 'Invalid token',
      },
    }, 401);
  }

  // 检查是否是卡密登录 (tg_user_id 格式为 "key:123")
  if (user.tg_user_id.startsWith('key:')) {
    const keyId = parseInt(user.tg_user_id.substring(4));
    
    // 查询卡密状态
    const key = await c.env.DB.prepare(
      'SELECT status, expires_at FROM license_keys WHERE id = ?'
    ).bind(keyId).first<{ status: string; expires_at: number | null }>();

    if (!key) {
      return c.json<ApiResponse>({
        success: false,
        error: {
          code: ErrorCodes.TOKEN_INVALID,
          message: '卡密不存在',
        },
      }, 401);
    }

    // 检查卡密是否被撤销
    if (key.status === 'revoked') {
      return c.json<ApiResponse>({
        success: false,
        error: {
          code: 'KEY_REVOKED',
          message: '卡密已被撤销',
        },
      }, 401);
    }

    // 检查卡密是否过期
    const now = Math.floor(Date.now() / 1000);
    if (key.status === 'expired' || (key.expires_at && key.expires_at < now)) {
      // 更新状态为过期
      if (key.status !== 'expired') {
        await c.env.DB.prepare(
          'UPDATE license_keys SET status = ? WHERE id = ?'
        ).bind('expired', keyId).run();
      }
      
      return c.json<ApiResponse>({
        success: false,
        error: {
          code: 'KEY_EXPIRED',
          message: '卡密已过期',
        },
      }, 401);
    }

    // 检查设备是否绑定
    const device = await c.env.DB.prepare(
      'SELECT is_active FROM key_devices WHERE key_id = ? AND device_id = ?'
    ).bind(keyId, user.device_id).first<{ is_active: number }>();

    if (!device || !device.is_active) {
      return c.json<ApiResponse>({
        success: false,
        error: {
          code: 'DEVICE_UNBOUND',
          message: '设备已解绑',
        },
      }, 401);
    }

    // 更新最后活跃时间
    await c.env.DB.prepare(
      'UPDATE key_devices SET last_seen = ? WHERE key_id = ? AND device_id = ?'
    ).bind(now, keyId, user.device_id).run();

    return c.json<ApiResponse<VerifyTokenResponse>>({
      success: true,
      data: {
        valid: true,
        expires_at: user.exp,
        key_expires_at: key.expires_at || undefined,
      },
    });
  }

  // 普通 Telegram 登录
  // 获取用户信息
  const userInfo = await c.env.DB.prepare(
    'SELECT tg_user_id, username FROM users WHERE tg_user_id = ?'
  ).bind(user.tg_user_id).first<{ tg_user_id: string; username: string | null }>();

  return c.json<ApiResponse<VerifyTokenResponse>>({
    success: true,
    data: {
      valid: true,
      user: userInfo ? {
        tg_user_id: userInfo.tg_user_id,
        username: userInfo.username,
      } : undefined,
      expires_at: user.exp,
    },
  });
});

// POST /api/approve_login - 批准登录 (Bot 调用)
authRoutes.post('/approve_login', async (c) => {
  // 验证 Bot Secret
  const botSecret = c.req.header('X-Bot-Secret');
  if (botSecret !== c.env.BOT_SECRET) {
    return c.json<ApiResponse>({
      success: false,
      error: {
        code: ErrorCodes.UNAUTHORIZED,
        message: 'Invalid bot secret',
      },
    }, 401);
  }

  const body = await c.req.json<{ session_id: string; tg_user_id: string; username?: string; first_name?: string }>().catch(() => null);
  
  if (!body?.session_id || !body?.tg_user_id) {
    return c.json<ApiResponse>({
      success: false,
      error: {
        code: ErrorCodes.BAD_REQUEST,
        message: 'session_id and tg_user_id are required',
      },
    }, 400);
  }

  const sessionService = new SessionService(c.env);
  const deviceService = new DeviceService(c.env);
  const auditService = new AuditService(c.env);
  
  try {
    // 获取会话
    const session = await sessionService.getSession(body.session_id);
    
    if (!session) {
      return c.json<ApiResponse>({
        success: false,
        error: {
          code: ErrorCodes.SESSION_NOT_FOUND,
          message: 'Session not found',
        },
      }, 404);
    }

    if (session.expires_at < Date.now() / 1000) {
      return c.json<ApiResponse>({
        success: false,
        error: {
          code: ErrorCodes.SESSION_EXPIRED,
          message: 'Session has expired',
        },
      }, 410);
    }

    // 检查用户是否被封禁
    const user = await c.env.DB.prepare(
      'SELECT is_banned, device_limit FROM users WHERE tg_user_id = ?'
    ).bind(body.tg_user_id).first<{ is_banned: number; device_limit: number }>();

    if (user?.is_banned) {
      return c.json<ApiResponse>({
        success: false,
        error: {
          code: ErrorCodes.USER_BANNED,
          message: 'User is banned',
        },
      }, 403);
    }

    // 检查设备绑定
    const deviceLimit = user?.device_limit || parseInt(c.env.DEFAULT_DEVICE_LIMIT);
    const canBind = await deviceService.canBindDevice(body.tg_user_id, session.device_id, deviceLimit);
    
    if (!canBind) {
      return c.json<ApiResponse>({
        success: false,
        error: {
          code: ErrorCodes.DEVICE_LIMIT_EXCEEDED,
          message: 'Device limit exceeded',
        },
      }, 403);
    }

    // 创建或更新用户
    await c.env.DB.prepare(`
      INSERT INTO users (tg_user_id, username, first_name, is_banned, device_limit, created_at, updated_at)
      VALUES (?, ?, ?, 0, ?, ?, ?)
      ON CONFLICT(tg_user_id) DO UPDATE SET
        username = excluded.username,
        first_name = excluded.first_name,
        updated_at = excluded.updated_at
    `).bind(
      body.tg_user_id,
      body.username || null,
      body.first_name || null,
      deviceLimit,
      Math.floor(Date.now() / 1000),
      Math.floor(Date.now() / 1000)
    ).run();

    // 绑定设备
    await deviceService.bindDevice(body.tg_user_id, session.device_id);

    // 更新会话状态
    await sessionService.approveSession(body.session_id, body.tg_user_id);

    // 记录审计日志
    await auditService.log({
      action: 'approve_login',
      tg_user_id: body.tg_user_id,
      device_id: session.device_id,
      details: JSON.stringify({ session_id: body.session_id }),
    });

    return c.json<ApiResponse>({
      success: true,
      data: {
        message: 'Login approved',
      },
    });
  } catch (error) {
    console.error('Error approving login:', error);
    return c.json<ApiResponse>({
      success: false,
      error: {
        code: ErrorCodes.INTERNAL_ERROR,
        message: 'Failed to approve login',
      },
    }, 500);
  }
});

// POST /api/reject_login - 拒绝登录 (Bot 调用)
authRoutes.post('/reject_login', async (c) => {
  // 验证 Bot Secret
  const botSecret = c.req.header('X-Bot-Secret');
  if (botSecret !== c.env.BOT_SECRET) {
    return c.json<ApiResponse>({
      success: false,
      error: {
        code: ErrorCodes.UNAUTHORIZED,
        message: 'Invalid bot secret',
      },
    }, 401);
  }

  const body = await c.req.json<{ session_id: string }>().catch(() => null);
  
  if (!body?.session_id) {
    return c.json<ApiResponse>({
      success: false,
      error: {
        code: ErrorCodes.BAD_REQUEST,
        message: 'session_id is required',
      },
    }, 400);
  }

  const sessionService = new SessionService(c.env);
  
  try {
    await sessionService.rejectSession(body.session_id);

    return c.json<ApiResponse>({
      success: true,
      data: {
        message: 'Login rejected',
      },
    });
  } catch (error) {
    console.error('Error rejecting login:', error);
    return c.json<ApiResponse>({
      success: false,
      error: {
        code: ErrorCodes.INTERNAL_ERROR,
        message: 'Failed to reject login',
      },
    }, 500);
  }
});
