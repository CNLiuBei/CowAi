// 卡密 API 路由
import { Hono } from 'hono';
import { Env, ApiResponse, ErrorCodes } from '../types';
import { LicenseService } from '../services/license';
import { JWTService } from '../services/jwt';
import { AuditService } from '../services/audit';

export const licenseRoutes = new Hono<{ Bindings: Env }>();

// POST /api/activate_key - 激活卡密
licenseRoutes.post('/activate_key', async (c) => {
  const body = await c.req.json<{ key_code: string; device_id: string }>().catch(() => null);
  
  if (!body?.key_code || !body?.device_id) {
    return c.json<ApiResponse>({
      success: false,
      error: {
        code: ErrorCodes.BAD_REQUEST,
        message: 'key_code and device_id are required',
      },
    }, 400);
  }

  const licenseService = new LicenseService(c.env);
  const jwtService = new JWTService(c.env);
  const auditService = new AuditService(c.env);

  try {
    const result = await licenseService.activateKey(body.key_code, body.device_id);

    if (!result.success) {
      await auditService.log({
        action: 'activate_key_failed',
        device_id: body.device_id,
        ip_address: c.req.header('CF-Connecting-IP') || null,
        details: JSON.stringify({ key_code: body.key_code.substring(0, 8) + '...', error: result.error }),
      });

      return c.json<ApiResponse>({
        success: false,
        error: {
          code: ErrorCodes.UNAUTHORIZED,
          message: result.error || 'Activation failed',
        },
      }, 401);
    }

    // 生成 JWT Token (使用 key_id 作为用户标识)
    const token = await jwtService.generateToken({
      tg_user_id: `key:${result.key!.id}`,
      device_id: body.device_id,
    });

    await auditService.log({
      action: 'activate_key',
      device_id: body.device_id,
      ip_address: c.req.header('CF-Connecting-IP') || null,
      details: JSON.stringify({ key_id: result.key!.id }),
    });

    return c.json<ApiResponse>({
      success: true,
      data: {
        access_token: token.token,
        expires_at: result.expires_at || token.expires_at,
        key_type: result.key!.key_type,
        key_expires_at: result.expires_at,
      },
    });
  } catch (error) {
    console.error('Error activating key:', error);
    return c.json<ApiResponse>({
      success: false,
      error: {
        code: ErrorCodes.INTERNAL_ERROR,
        message: 'Failed to activate key',
      },
    }, 500);
  }
});

// POST /api/verify_key - 验证卡密
licenseRoutes.post('/verify_key', async (c) => {
  const body = await c.req.json<{ key_code: string; device_id: string }>().catch(() => null);
  
  if (!body?.key_code || !body?.device_id) {
    return c.json<ApiResponse>({
      success: false,
      error: {
        code: ErrorCodes.BAD_REQUEST,
        message: 'key_code and device_id are required',
      },
    }, 400);
  }

  const licenseService = new LicenseService(c.env);

  try {
    const result = await licenseService.verifyKey(body.key_code, body.device_id);

    if (!result.success) {
      return c.json<ApiResponse>({
        success: false,
        error: {
          code: ErrorCodes.UNAUTHORIZED,
          message: result.error || 'Verification failed',
        },
      }, 401);
    }

    return c.json<ApiResponse>({
      success: true,
      data: {
        valid: true,
        key_type: result.key!.key_type,
        expires_at: result.expires_at,
      },
    });
  } catch (error) {
    console.error('Error verifying key:', error);
    return c.json<ApiResponse>({
      success: false,
      error: {
        code: ErrorCodes.INTERNAL_ERROR,
        message: 'Failed to verify key',
      },
    }, 500);
  }
});

// GET /api/key_status - 查询卡密状态 (需要 device_id)
licenseRoutes.get('/key_status', async (c) => {
  const keyCode = c.req.query('key_code');
  const deviceId = c.req.query('device_id');
  
  if (!keyCode) {
    return c.json<ApiResponse>({
      success: false,
      error: {
        code: ErrorCodes.BAD_REQUEST,
        message: 'key_code is required',
      },
    }, 400);
  }

  const licenseService = new LicenseService(c.env);

  try {
    const key = await licenseService.getKey(keyCode);

    if (!key) {
      return c.json<ApiResponse>({
        success: false,
        error: {
          code: ErrorCodes.NOT_FOUND,
          message: 'Key not found',
        },
      }, 404);
    }

    // 检查是否过期
    const now = Math.floor(Date.now() / 1000);
    let status = key.status;
    if (status === 'activated' && key.expires_at && key.expires_at < now) {
      status = 'expired';
    }

    // 获取设备绑定信息
    const devices = await licenseService.getKeyDevices(key.id);
    const isDeviceBound = deviceId ? devices.some(d => d.device_id === deviceId && d.is_active) : undefined;

    return c.json<ApiResponse>({
      success: true,
      data: {
        status,
        key_type: key.key_type,
        max_devices: key.max_devices,
        bound_devices: devices.filter(d => d.is_active).length,
        expires_at: key.expires_at,
        is_device_bound: isDeviceBound,
      },
    });
  } catch (error) {
    console.error('Error getting key status:', error);
    return c.json<ApiResponse>({
      success: false,
      error: {
        code: ErrorCodes.INTERNAL_ERROR,
        message: 'Failed to get key status',
      },
    }, 500);
  }
});

// POST /api/admin/generate_keys - 生成卡密 (管理员)
licenseRoutes.post('/admin/generate_keys', async (c) => {
  // 验证 Bot Secret (管理员操作)
  const botSecret = c.req.header('X-Bot-Secret');
  if (botSecret !== c.env.BOT_SECRET) {
    return c.json<ApiResponse>({
      success: false,
      error: {
        code: ErrorCodes.UNAUTHORIZED,
        message: 'Unauthorized',
      },
    }, 401);
  }

  const body = await c.req.json<{
    count: number;
    key_type: 'time' | 'permanent';
    duration_days?: number;
    max_devices?: number;
    created_by: string;
    note?: string;
  }>().catch(() => null);

  if (!body?.count || !body?.key_type || !body?.created_by) {
    return c.json<ApiResponse>({
      success: false,
      error: {
        code: ErrorCodes.BAD_REQUEST,
        message: 'count, key_type, and created_by are required',
      },
    }, 400);
  }

  if (body.count > 100) {
    return c.json<ApiResponse>({
      success: false,
      error: {
        code: ErrorCodes.BAD_REQUEST,
        message: 'Maximum 100 keys per request',
      },
    }, 400);
  }

  const licenseService = new LicenseService(c.env);

  try {
    const keys = await licenseService.generateKeys({
      count: body.count,
      keyType: body.key_type,
      durationDays: body.duration_days,
      maxDevices: body.max_devices,
      createdBy: body.created_by,
      note: body.note,
    });

    return c.json<ApiResponse>({
      success: true,
      data: {
        keys,
        count: keys.length,
      },
    });
  } catch (error) {
    console.error('Error generating keys:', error);
    return c.json<ApiResponse>({
      success: false,
      error: {
        code: ErrorCodes.INTERNAL_ERROR,
        message: 'Failed to generate keys',
      },
    }, 500);
  }
});

// POST /api/admin/revoke_key - 撤销卡密 (管理员)
licenseRoutes.post('/admin/revoke_key', async (c) => {
  const botSecret = c.req.header('X-Bot-Secret');
  if (botSecret !== c.env.BOT_SECRET) {
    return c.json<ApiResponse>({
      success: false,
      error: {
        code: ErrorCodes.UNAUTHORIZED,
        message: 'Unauthorized',
      },
    }, 401);
  }

  const body = await c.req.json<{ key_code: string }>().catch(() => null);

  if (!body?.key_code) {
    return c.json<ApiResponse>({
      success: false,
      error: {
        code: ErrorCodes.BAD_REQUEST,
        message: 'key_code is required',
      },
    }, 400);
  }

  const licenseService = new LicenseService(c.env);

  try {
    const success = await licenseService.revokeKey(body.key_code);

    if (!success) {
      return c.json<ApiResponse>({
        success: false,
        error: {
          code: ErrorCodes.NOT_FOUND,
          message: 'Key not found',
        },
      }, 404);
    }

    return c.json<ApiResponse>({
      success: true,
      data: { message: 'Key revoked' },
    });
  } catch (error) {
    console.error('Error revoking key:', error);
    return c.json<ApiResponse>({
      success: false,
      error: {
        code: ErrorCodes.INTERNAL_ERROR,
        message: 'Failed to revoke key',
      },
    }, 500);
  }
});

// GET /api/admin/keys - 获取卡密列表 (管理员)
licenseRoutes.get('/admin/keys', async (c) => {
  const botSecret = c.req.header('X-Bot-Secret');
  if (botSecret !== c.env.BOT_SECRET) {
    return c.json<ApiResponse>({
      success: false,
      error: {
        code: ErrorCodes.UNAUTHORIZED,
        message: 'Unauthorized',
      },
    }, 401);
  }

  const status = c.req.query('status');
  const limit = parseInt(c.req.query('limit') || '50');
  const offset = parseInt(c.req.query('offset') || '0');

  const licenseService = new LicenseService(c.env);

  try {
    const result = await licenseService.listKeys({ status, limit, offset });

    return c.json<ApiResponse>({
      success: true,
      data: result,
    });
  } catch (error) {
    console.error('Error listing keys:', error);
    return c.json<ApiResponse>({
      success: false,
      error: {
        code: ErrorCodes.INTERNAL_ERROR,
        message: 'Failed to list keys',
      },
    }, 500);
  }
});

// GET /api/admin/key_stats - 卡密统计 (管理员)
licenseRoutes.get('/admin/key_stats', async (c) => {
  const botSecret = c.req.header('X-Bot-Secret');
  if (botSecret !== c.env.BOT_SECRET) {
    return c.json<ApiResponse>({
      success: false,
      error: {
        code: ErrorCodes.UNAUTHORIZED,
        message: 'Unauthorized',
      },
    }, 401);
  }

  const licenseService = new LicenseService(c.env);

  try {
    const stats = await licenseService.getStats();

    return c.json<ApiResponse>({
      success: true,
      data: stats,
    });
  } catch (error) {
    console.error('Error getting key stats:', error);
    return c.json<ApiResponse>({
      success: false,
      error: {
        code: ErrorCodes.INTERNAL_ERROR,
        message: 'Failed to get key stats',
      },
    }, 500);
  }
});

// GET /api/admin/key/:keyCode - 获取卡密详情 (管理员)
licenseRoutes.get('/admin/key/:keyCode', async (c) => {
  const botSecret = c.req.header('X-Bot-Secret');
  if (botSecret !== c.env.BOT_SECRET) {
    return c.json<ApiResponse>({
      success: false,
      error: {
        code: ErrorCodes.UNAUTHORIZED,
        message: 'Unauthorized',
      },
    }, 401);
  }

  const keyCode = c.req.param('keyCode');
  const licenseService = new LicenseService(c.env);

  try {
    const key = await licenseService.getKey(keyCode);

    if (!key) {
      return c.json<ApiResponse>({
        success: false,
        error: {
          code: ErrorCodes.NOT_FOUND,
          message: 'Key not found',
        },
      }, 404);
    }

    const devices = await licenseService.getKeyDevices(key.id);

    return c.json<ApiResponse>({
      success: true,
      data: {
        key,
        devices,
      },
    });
  } catch (error) {
    console.error('Error getting key details:', error);
    return c.json<ApiResponse>({
      success: false,
      error: {
        code: ErrorCodes.INTERNAL_ERROR,
        message: 'Failed to get key details',
      },
    }, 500);
  }
});

// POST /api/admin/key/:keyCode/unbind - 解绑设备 (管理员)
licenseRoutes.post('/admin/key/:keyCode/unbind', async (c) => {
  const botSecret = c.req.header('X-Bot-Secret');
  if (botSecret !== c.env.BOT_SECRET) {
    return c.json<ApiResponse>({
      success: false,
      error: {
        code: ErrorCodes.UNAUTHORIZED,
        message: 'Unauthorized',
      },
    }, 401);
  }

  const keyCode = c.req.param('keyCode');
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

  const licenseService = new LicenseService(c.env);

  try {
    const success = await licenseService.unbindDevice(keyCode, body.device_id);

    if (!success) {
      return c.json<ApiResponse>({
        success: false,
        error: {
          code: ErrorCodes.NOT_FOUND,
          message: 'Key or device not found',
        },
      }, 404);
    }

    return c.json<ApiResponse>({
      success: true,
      data: { message: 'Device unbound' },
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

// GET /api/admin/products - 获取商品列表 (管理员)
licenseRoutes.get('/admin/products', async (c) => {
  const botSecret = c.req.header('X-Bot-Secret');
  if (botSecret !== c.env.BOT_SECRET) {
    return c.json<ApiResponse>({
      success: false,
      error: { code: ErrorCodes.UNAUTHORIZED, message: 'Unauthorized' },
    }, 401);
  }

  try {
    const result = await c.env.DB.prepare(
      'SELECT * FROM products ORDER BY sort_order ASC, price ASC'
    ).all();

    return c.json<ApiResponse>({
      success: true,
      data: { products: result.results || [] },
    });
  } catch (error) {
    console.error('Error getting products:', error);
    return c.json<ApiResponse>({
      success: false,
      error: { code: ErrorCodes.INTERNAL_ERROR, message: 'Failed to get products' },
    }, 500);
  }
});

// POST /api/admin/products - 创建商品 (管理员)
licenseRoutes.post('/admin/products', async (c) => {
  const botSecret = c.req.header('X-Bot-Secret');
  if (botSecret !== c.env.BOT_SECRET) {
    return c.json<ApiResponse>({
      success: false,
      error: { code: ErrorCodes.UNAUTHORIZED, message: 'Unauthorized' },
    }, 401);
  }

  const body = await c.req.json<{
    id: string;
    name: string;
    description?: string | null;
    price: number;
    key_type: 'time' | 'permanent';
    duration_days?: number | null;
    max_devices: number;
    sort_order?: number;
  }>().catch(() => null);

  if (!body?.id || !body?.name || !body?.price || !body?.key_type) {
    return c.json<ApiResponse>({
      success: false,
      error: { code: ErrorCodes.BAD_REQUEST, message: 'Missing required fields' },
    }, 400);
  }

  try {
    const now = Math.floor(Date.now() / 1000);
    await c.env.DB.prepare(
      `INSERT INTO products (id, name, description, price, key_type, duration_days, max_devices, is_active, sort_order, created_at)
       VALUES (?, ?, ?, ?, ?, ?, ?, 1, ?, ?)`
    ).bind(
      body.id,
      body.name,
      body.description || null,
      body.price,
      body.key_type,
      body.duration_days || null,
      body.max_devices || 1,
      body.sort_order || 0,
      now
    ).run();

    return c.json<ApiResponse>({ success: true, data: { message: 'Product created' } });
  } catch (error) {
    console.error('Error creating product:', error);
    return c.json<ApiResponse>({
      success: false,
      error: { code: ErrorCodes.INTERNAL_ERROR, message: 'Failed to create product' },
    }, 500);
  }
});

// PUT /api/admin/products/:id - 更新商品 (管理员)
licenseRoutes.put('/admin/products/:id', async (c) => {
  const botSecret = c.req.header('X-Bot-Secret');
  if (botSecret !== c.env.BOT_SECRET) {
    return c.json<ApiResponse>({
      success: false,
      error: { code: ErrorCodes.UNAUTHORIZED, message: 'Unauthorized' },
    }, 401);
  }

  const productId = c.req.param('id');
  const body = await c.req.json<Partial<{
    name: string;
    description: string | null;
    price: number;
    key_type: 'time' | 'permanent';
    duration_days: number | null;
    max_devices: number;
    is_active: number;
    sort_order: number;
  }>>().catch(() => null);

  if (!body) {
    return c.json<ApiResponse>({
      success: false,
      error: { code: ErrorCodes.BAD_REQUEST, message: 'Invalid request body' },
    }, 400);
  }

  try {
    const updates: string[] = [];
    const values: (string | number | null)[] = [];

    if (body.name !== undefined) { updates.push('name = ?'); values.push(body.name); }
    if (body.description !== undefined) { updates.push('description = ?'); values.push(body.description); }
    if (body.price !== undefined) { updates.push('price = ?'); values.push(body.price); }
    if (body.key_type !== undefined) { updates.push('key_type = ?'); values.push(body.key_type); }
    if (body.duration_days !== undefined) { updates.push('duration_days = ?'); values.push(body.duration_days); }
    if (body.max_devices !== undefined) { updates.push('max_devices = ?'); values.push(body.max_devices); }
    if (body.is_active !== undefined) { updates.push('is_active = ?'); values.push(body.is_active); }
    if (body.sort_order !== undefined) { updates.push('sort_order = ?'); values.push(body.sort_order); }

    if (updates.length === 0) {
      return c.json<ApiResponse>({ success: true, data: { message: 'No changes' } });
    }

    values.push(productId);
    await c.env.DB.prepare(
      `UPDATE products SET ${updates.join(', ')} WHERE id = ?`
    ).bind(...values).run();

    return c.json<ApiResponse>({ success: true, data: { message: 'Product updated' } });
  } catch (error) {
    console.error('Error updating product:', error);
    return c.json<ApiResponse>({
      success: false,
      error: { code: ErrorCodes.INTERNAL_ERROR, message: 'Failed to update product' },
    }, 500);
  }
});

// DELETE /api/admin/products/:id - 删除商品 (管理员)
licenseRoutes.delete('/admin/products/:id', async (c) => {
  const botSecret = c.req.header('X-Bot-Secret');
  if (botSecret !== c.env.BOT_SECRET) {
    return c.json<ApiResponse>({
      success: false,
      error: { code: ErrorCodes.UNAUTHORIZED, message: 'Unauthorized' },
    }, 401);
  }

  const productId = c.req.param('id');

  try {
    await c.env.DB.prepare('DELETE FROM products WHERE id = ?').bind(productId).run();
    return c.json<ApiResponse>({ success: true, data: { message: 'Product deleted' } });
  } catch (error) {
    console.error('Error deleting product:', error);
    return c.json<ApiResponse>({
      success: false,
      error: { code: ErrorCodes.INTERNAL_ERROR, message: 'Failed to delete product' },
    }, 500);
  }
});
