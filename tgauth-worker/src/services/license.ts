// 卡密服务
import { Env } from '../types';

export interface LicenseKey {
  id: number;
  key_code: string;
  key_type: 'time' | 'permanent';
  duration_days: number | null;
  max_devices: number;
  created_by: string;
  created_at: number;
  activated_by: string | null;
  activated_at: number | null;
  expires_at: number | null;
  status: 'unused' | 'activated' | 'expired' | 'revoked';
  note: string | null;
}

export interface ActivateResult {
  success: boolean;
  error?: string;
  key?: LicenseKey;
  expires_at?: number;
}

export class LicenseService {
  constructor(private env: Env) {}

  // 生成随机卡密
  private generateKeyCode(length: number = 16): string {
    const chars = 'ABCDEFGHJKLMNPQRSTUVWXYZ23456789'; // 去掉容易混淆的字符
    let result = '';
    const array = new Uint8Array(length);
    crypto.getRandomValues(array);
    for (let i = 0; i < length; i++) {
      result += chars[array[i] % chars.length];
    }
    // 格式化为 XXXX-XXXX-XXXX-XXXX
    return result.match(/.{1,4}/g)?.join('-') || result;
  }

  // 批量生成卡密
  async generateKeys(params: {
    count: number;
    keyType: 'time' | 'permanent';
    durationDays?: number;
    maxDevices?: number;
    createdBy: string;
    note?: string;
  }): Promise<string[]> {
    const keys: string[] = [];
    const now = Math.floor(Date.now() / 1000);

    for (let i = 0; i < params.count; i++) {
      const keyCode = this.generateKeyCode();
      
      await this.env.DB.prepare(`
        INSERT INTO license_keys (key_code, key_type, duration_days, max_devices, created_by, created_at, status, note)
        VALUES (?, ?, ?, ?, ?, ?, 'unused', ?)
      `).bind(
        keyCode,
        params.keyType,
        params.keyType === 'time' ? (params.durationDays || 30) : null,
        params.maxDevices || 1,
        params.createdBy,
        now,
        params.note || null
      ).run();

      keys.push(keyCode);
    }

    return keys;
  }

  // 获取卡密信息
  async getKey(keyCode: string): Promise<LicenseKey | null> {
    const key = await this.env.DB.prepare(
      'SELECT * FROM license_keys WHERE key_code = ?'
    ).bind(keyCode.toUpperCase().replace(/[^A-Z0-9]/g, '')).first<LicenseKey>();
    
    // 也尝试带横杠的格式
    if (!key) {
      return await this.env.DB.prepare(
        'SELECT * FROM license_keys WHERE key_code = ?'
      ).bind(keyCode.toUpperCase()).first<LicenseKey>();
    }
    
    return key;
  }

  // 激活卡密
  async activateKey(keyCode: string, deviceId: string): Promise<ActivateResult> {
    // 标准化卡密格式
    const normalizedKey = keyCode.toUpperCase().trim();
    
    const key = await this.getKey(normalizedKey);
    
    if (!key) {
      return { success: false, error: '卡密不存在' };
    }

    if (key.status === 'revoked') {
      return { success: false, error: '卡密已被撤销' };
    }

    if (key.status === 'expired') {
      return { success: false, error: '卡密已过期' };
    }

    const now = Math.floor(Date.now() / 1000);

    // 检查是否已过期 (针对已激活的卡密)
    if (key.status === 'activated' && key.expires_at && key.expires_at < now) {
      await this.env.DB.prepare(
        'UPDATE license_keys SET status = ? WHERE id = ?'
      ).bind('expired', key.id).run();
      return { success: false, error: '卡密已过期' };
    }

    // 如果是未使用的卡密，进行首次激活
    if (key.status === 'unused') {
      const expiresAt = key.key_type === 'permanent' 
        ? null 
        : now + (key.duration_days || 30) * 24 * 60 * 60;

      await this.env.DB.prepare(`
        UPDATE license_keys 
        SET status = 'activated', activated_by = ?, activated_at = ?, expires_at = ?
        WHERE id = ?
      `).bind(deviceId, now, expiresAt, key.id).run();

      // 绑定设备
      await this.env.DB.prepare(`
        INSERT INTO key_devices (key_id, device_id, bound_at, last_seen, is_active)
        VALUES (?, ?, ?, ?, 1)
      `).bind(key.id, deviceId, now, now).run();

      return { 
        success: true, 
        key: { ...key, status: 'activated', expires_at: expiresAt },
        expires_at: expiresAt || undefined
      };
    }

    // 已激活的卡密，检查设备绑定
    const boundDevices = await this.env.DB.prepare(
      'SELECT COUNT(*) as count FROM key_devices WHERE key_id = ? AND is_active = 1'
    ).bind(key.id).first<{ count: number }>();

    const existingDevice = await this.env.DB.prepare(
      'SELECT * FROM key_devices WHERE key_id = ? AND device_id = ?'
    ).bind(key.id, deviceId).first();

    if (existingDevice) {
      // 设备已绑定，更新最后活跃时间
      await this.env.DB.prepare(
        'UPDATE key_devices SET last_seen = ?, is_active = 1 WHERE key_id = ? AND device_id = ?'
      ).bind(now, key.id, deviceId).run();
      
      return { 
        success: true, 
        key,
        expires_at: key.expires_at || undefined
      };
    }

    // 检查设备数量限制
    if ((boundDevices?.count || 0) >= key.max_devices) {
      return { success: false, error: `设备数量已达上限 (${key.max_devices})` };
    }

    // 绑定新设备
    await this.env.DB.prepare(`
      INSERT INTO key_devices (key_id, device_id, bound_at, last_seen, is_active)
      VALUES (?, ?, ?, ?, 1)
    `).bind(key.id, deviceId, now, now).run();

    return { 
      success: true, 
      key,
      expires_at: key.expires_at || undefined
    };
  }

  // 验证卡密有效性
  async verifyKey(keyCode: string, deviceId: string): Promise<ActivateResult> {
    const key = await this.getKey(keyCode);
    
    if (!key) {
      return { success: false, error: '卡密不存在' };
    }

    if (key.status !== 'activated') {
      return { success: false, error: '卡密未激活' };
    }

    const now = Math.floor(Date.now() / 1000);

    // 检查是否过期
    if (key.expires_at && key.expires_at < now) {
      await this.env.DB.prepare(
        'UPDATE license_keys SET status = ? WHERE id = ?'
      ).bind('expired', key.id).run();
      return { success: false, error: '卡密已过期' };
    }

    // 检查设备是否绑定
    const device = await this.env.DB.prepare(
      'SELECT * FROM key_devices WHERE key_id = ? AND device_id = ? AND is_active = 1'
    ).bind(key.id, deviceId).first();

    if (!device) {
      return { success: false, error: '设备未绑定此卡密' };
    }

    // 更新最后活跃时间
    await this.env.DB.prepare(
      'UPDATE key_devices SET last_seen = ? WHERE key_id = ? AND device_id = ?'
    ).bind(now, key.id, deviceId).run();

    return { 
      success: true, 
      key,
      expires_at: key.expires_at || undefined
    };
  }

  // 撤销卡密
  async revokeKey(keyCode: string): Promise<boolean> {
    const result = await this.env.DB.prepare(
      'UPDATE license_keys SET status = ? WHERE key_code = ?'
    ).bind('revoked', keyCode).run();
    
    return result.meta.changes > 0;
  }

  // 获取卡密列表
  async listKeys(params: {
    status?: string;
    createdBy?: string;
    limit?: number;
    offset?: number;
  }): Promise<{ keys: LicenseKey[]; total: number }> {
    let query = 'SELECT * FROM license_keys WHERE 1=1';
    let countQuery = 'SELECT COUNT(*) as total FROM license_keys WHERE 1=1';
    const bindings: any[] = [];

    if (params.status) {
      query += ' AND status = ?';
      countQuery += ' AND status = ?';
      bindings.push(params.status);
    }

    if (params.createdBy) {
      query += ' AND created_by = ?';
      countQuery += ' AND created_by = ?';
      bindings.push(params.createdBy);
    }

    query += ' ORDER BY created_at DESC';
    query += ` LIMIT ${params.limit || 50} OFFSET ${params.offset || 0}`;

    const [keys, count] = await Promise.all([
      this.env.DB.prepare(query).bind(...bindings).all<LicenseKey>(),
      this.env.DB.prepare(countQuery).bind(...bindings.slice(0, params.status ? (params.createdBy ? 2 : 1) : (params.createdBy ? 1 : 0))).first<{ total: number }>()
    ]);

    return {
      keys: keys.results || [],
      total: count?.total || 0
    };
  }

  // 获取卡密绑定的设备
  async getKeyDevices(keyId: number): Promise<Array<{ device_id: string; bound_at: number; last_seen: number; is_active: number }>> {
    const result = await this.env.DB.prepare(
      'SELECT device_id, bound_at, last_seen, is_active FROM key_devices WHERE key_id = ?'
    ).bind(keyId).all<{ device_id: string; bound_at: number; last_seen: number; is_active: number }>();
    
    return result.results || [];
  }

  // 解绑设备
  async unbindDevice(keyCode: string, deviceId: string): Promise<boolean> {
    const key = await this.getKey(keyCode);
    if (!key) return false;

    const result = await this.env.DB.prepare(
      'UPDATE key_devices SET is_active = 0 WHERE key_id = ? AND device_id = ?'
    ).bind(key.id, deviceId).run();

    return result.meta.changes > 0;
  }

  // 获取统计信息
  async getStats(): Promise<{
    total: number;
    unused: number;
    activated: number;
    expired: number;
    revoked: number;
  }> {
    const result = await this.env.DB.prepare(`
      SELECT 
        COUNT(*) as total,
        SUM(CASE WHEN status = 'unused' THEN 1 ELSE 0 END) as unused,
        SUM(CASE WHEN status = 'activated' THEN 1 ELSE 0 END) as activated,
        SUM(CASE WHEN status = 'expired' THEN 1 ELSE 0 END) as expired,
        SUM(CASE WHEN status = 'revoked' THEN 1 ELSE 0 END) as revoked
      FROM license_keys
    `).first<{ total: number; unused: number; activated: number; expired: number; revoked: number }>();

    return result || { total: 0, unused: 0, activated: 0, expired: 0, revoked: 0 };
  }
}
