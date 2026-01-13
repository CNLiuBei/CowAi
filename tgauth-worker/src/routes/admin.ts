import { Hono } from 'hono';
import { Env, ApiResponse, ErrorCodes } from '../types';
import { DeviceService } from '../services/device';
import { AuditService } from '../services/audit';
import { adminAuthMiddleware } from '../middleware/auth';

export const adminRoutes = new Hono<{ Bindings: Env }>();

// 所有管理员路由都需要验证
adminRoutes.use('*', adminAuthMiddleware);

// POST /api/admin/ban - 封禁用户
adminRoutes.post('/ban', async (c) => {
  const body = await c.req.json<{ tg_user_id: string }>().catch(() => null);
  
  if (!body?.tg_user_id) {
    return c.json<ApiResponse>({
      success: false,
      error: {
        code: ErrorCodes.BAD_REQUEST,
        message: 'tg_user_id is required',
      },
    }, 400);
  }

  const auditService = new AuditService(c.env);
  
  try {
    // 更新用户封禁状态
    await c.env.DB.prepare(
      'UPDATE users SET is_banned = 1, updated_at = ? WHERE tg_user_id = ?'
    ).bind(Math.floor(Date.now() / 1000), body.tg_user_id).run();

    // 将该用户的所有 token 加入黑名单
    // 通过在 KV 中存储封禁标记实现
    await c.env.KV.put(`banned:${body.tg_user_id}`, '1', {
      expirationTtl: 60 * 60 * 24 * 365, // 1 年
    });

    // 记录审计日志
    const adminId = c.get('adminId' as never) as string;
    await auditService.log({
      action: 'ban_user',
      tg_user_id: body.tg_user_id,
      details: JSON.stringify({ admin_id: adminId }),
    });

    return c.json<ApiResponse>({
      success: true,
      data: {
        message: `User ${body.tg_user_id} has been banned`,
      },
    });
  } catch (error) {
    console.error('Error banning user:', error);
    return c.json<ApiResponse>({
      success: false,
      error: {
        code: ErrorCodes.INTERNAL_ERROR,
        message: 'Failed to ban user',
      },
    }, 500);
  }
});

// POST /api/admin/unban - 解封用户
adminRoutes.post('/unban', async (c) => {
  const body = await c.req.json<{ tg_user_id: string }>().catch(() => null);
  
  if (!body?.tg_user_id) {
    return c.json<ApiResponse>({
      success: false,
      error: {
        code: ErrorCodes.BAD_REQUEST,
        message: 'tg_user_id is required',
      },
    }, 400);
  }

  const auditService = new AuditService(c.env);
  
  try {
    // 更新用户封禁状态
    await c.env.DB.prepare(
      'UPDATE users SET is_banned = 0, updated_at = ? WHERE tg_user_id = ?'
    ).bind(Math.floor(Date.now() / 1000), body.tg_user_id).run();

    // 移除封禁标记
    await c.env.KV.delete(`banned:${body.tg_user_id}`);

    // 记录审计日志
    const adminId = c.get('adminId' as never) as string;
    await auditService.log({
      action: 'unban_user',
      tg_user_id: body.tg_user_id,
      details: JSON.stringify({ admin_id: adminId }),
    });

    return c.json<ApiResponse>({
      success: true,
      data: {
        message: `User ${body.tg_user_id} has been unbanned`,
      },
    });
  } catch (error) {
    console.error('Error unbanning user:', error);
    return c.json<ApiResponse>({
      success: false,
      error: {
        code: ErrorCodes.INTERNAL_ERROR,
        message: 'Failed to unban user',
      },
    }, 500);
  }
});

// GET /api/admin/devices - 查询用户设备
adminRoutes.get('/devices', async (c) => {
  const tgUserId = c.req.query('tg_user_id');
  
  if (!tgUserId) {
    return c.json<ApiResponse>({
      success: false,
      error: {
        code: ErrorCodes.BAD_REQUEST,
        message: 'tg_user_id is required',
      },
    }, 400);
  }

  const deviceService = new DeviceService(c.env);
  
  try {
    const devices = await deviceService.getUserDevices(tgUserId);

    return c.json<ApiResponse>({
      success: true,
      data: {
        tg_user_id: tgUserId,
        devices: devices,
        count: devices.length,
      },
    });
  } catch (error) {
    console.error('Error getting devices:', error);
    return c.json<ApiResponse>({
      success: false,
      error: {
        code: ErrorCodes.INTERNAL_ERROR,
        message: 'Failed to get devices',
      },
    }, 500);
  }
});

// POST /api/admin/unbind - 解绑设备
adminRoutes.post('/unbind', async (c) => {
  const body = await c.req.json<{ tg_user_id: string; device_id: string }>().catch(() => null);
  
  if (!body?.tg_user_id || !body?.device_id) {
    return c.json<ApiResponse>({
      success: false,
      error: {
        code: ErrorCodes.BAD_REQUEST,
        message: 'tg_user_id and device_id are required',
      },
    }, 400);
  }

  const deviceService = new DeviceService(c.env);
  const auditService = new AuditService(c.env);
  
  try {
    await deviceService.unbindDevice(body.tg_user_id, body.device_id);

    // 将该设备的 token 加入黑名单
    await c.env.KV.put(`unbind:${body.device_id}`, '1', {
      expirationTtl: 60 * 60 * 24 * 7, // 7 天
    });

    // 记录审计日志
    const adminId = c.get('adminId' as never) as string;
    await auditService.log({
      action: 'unbind_device',
      tg_user_id: body.tg_user_id,
      device_id: body.device_id,
      details: JSON.stringify({ admin_id: adminId }),
    });

    return c.json<ApiResponse>({
      success: true,
      data: {
        message: `Device ${body.device_id} has been unbound from user ${body.tg_user_id}`,
      },
    });
  } catch (error) {
    console.error('Error unbinding device:', error);
    return c.json<ApiResponse>({
      success: false,
      error: {
        code: ErrorCodes.INTERNAL_ERROR,
        message: 'Failed to unbind device',
      },
    }, 500);
  }
});

// POST /api/admin/unbind_all - 解绑用户所有设备
adminRoutes.post('/unbind_all', async (c) => {
  const body = await c.req.json<{ tg_user_id: string }>().catch(() => null);
  
  if (!body?.tg_user_id) {
    return c.json<ApiResponse>({
      success: false,
      error: {
        code: ErrorCodes.BAD_REQUEST,
        message: 'tg_user_id is required',
      },
    }, 400);
  }

  const deviceService = new DeviceService(c.env);
  const auditService = new AuditService(c.env);
  
  try {
    // 获取所有设备
    const devices = await deviceService.getUserDevices(body.tg_user_id);
    
    // 解绑所有设备
    await deviceService.unbindAllDevices(body.tg_user_id);

    // 将所有设备的 token 加入黑名单
    for (const device of devices) {
      await c.env.KV.put(`unbind:${device.device_id}`, '1', {
        expirationTtl: 60 * 60 * 24 * 7, // 7 天
      });
    }

    // 记录审计日志
    const adminId = c.get('adminId' as never) as string;
    await auditService.log({
      action: 'unbind_all_devices',
      tg_user_id: body.tg_user_id,
      details: JSON.stringify({ admin_id: adminId, device_count: devices.length }),
    });

    return c.json<ApiResponse>({
      success: true,
      data: {
        message: `All devices (${devices.length}) have been unbound from user ${body.tg_user_id}`,
        count: devices.length,
      },
    });
  } catch (error) {
    console.error('Error unbinding all devices:', error);
    return c.json<ApiResponse>({
      success: false,
      error: {
        code: ErrorCodes.INTERNAL_ERROR,
        message: 'Failed to unbind devices',
      },
    }, 500);
  }
});

// GET /api/admin/stats - 系统统计
adminRoutes.get('/stats', async (c) => {
  try {
    const [usersResult, devicesResult, sessionsResult] = await Promise.all([
      c.env.DB.prepare('SELECT COUNT(*) as count FROM users').first<{ count: number }>(),
      c.env.DB.prepare('SELECT COUNT(*) as count FROM devices WHERE is_active = 1').first<{ count: number }>(),
      c.env.DB.prepare('SELECT COUNT(*) as count FROM sessions WHERE status = ?').bind('pending').first<{ count: number }>(),
    ]);

    return c.json<ApiResponse>({
      success: true,
      data: {
        total_users: usersResult?.count || 0,
        active_devices: devicesResult?.count || 0,
        pending_sessions: sessionsResult?.count || 0,
        timestamp: Date.now(),
      },
    });
  } catch (error) {
    console.error('Error getting stats:', error);
    return c.json<ApiResponse>({
      success: false,
      error: {
        code: ErrorCodes.INTERNAL_ERROR,
        message: 'Failed to get stats',
      },
    }, 500);
  }
});
