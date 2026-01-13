import { Env, Device } from '../types';

export class DeviceService {
  private env: Env;

  constructor(env: Env) {
    this.env = env;
  }

  // 检查是否可以绑定设备
  async canBindDevice(tgUserId: string, deviceId: string, deviceLimit: number): Promise<boolean> {
    // 检查设备是否已绑定到该用户
    const existingDevice = await this.env.DB.prepare(
      'SELECT id FROM devices WHERE tg_user_id = ? AND device_id = ? AND is_active = 1'
    ).bind(tgUserId, deviceId).first();

    if (existingDevice) {
      // 设备已绑定，允许登录
      return true;
    }

    // 检查用户已绑定的设备数量
    const deviceCount = await this.env.DB.prepare(
      'SELECT COUNT(*) as count FROM devices WHERE tg_user_id = ? AND is_active = 1'
    ).bind(tgUserId).first<{ count: number }>();

    const currentCount = deviceCount?.count || 0;
    return currentCount < deviceLimit;
  }

  // 绑定设备
  async bindDevice(tgUserId: string, deviceId: string, deviceInfo?: string): Promise<void> {
    const now = Math.floor(Date.now() / 1000);

    await this.env.DB.prepare(`
      INSERT INTO devices (device_id, tg_user_id, device_info, bound_at, last_seen, is_active)
      VALUES (?, ?, ?, ?, ?, 1)
      ON CONFLICT(device_id, tg_user_id) DO UPDATE SET
        last_seen = excluded.last_seen,
        is_active = 1
    `).bind(deviceId, tgUserId, deviceInfo || null, now, now).run();
  }

  // 更新设备最后活跃时间
  async updateLastSeen(tgUserId: string, deviceId: string): Promise<void> {
    const now = Math.floor(Date.now() / 1000);

    await this.env.DB.prepare(
      'UPDATE devices SET last_seen = ? WHERE tg_user_id = ? AND device_id = ?'
    ).bind(now, tgUserId, deviceId).run();
  }

  // 获取用户的所有设备
  async getUserDevices(tgUserId: string): Promise<Device[]> {
    const result = await this.env.DB.prepare(
      'SELECT * FROM devices WHERE tg_user_id = ? ORDER BY last_seen DESC'
    ).bind(tgUserId).all<Device>();

    return result.results || [];
  }

  // 获取用户的活跃设备
  async getActiveDevices(tgUserId: string): Promise<Device[]> {
    const result = await this.env.DB.prepare(
      'SELECT * FROM devices WHERE tg_user_id = ? AND is_active = 1 ORDER BY last_seen DESC'
    ).bind(tgUserId).all<Device>();

    return result.results || [];
  }

  // 解绑设备
  async unbindDevice(tgUserId: string, deviceId: string): Promise<void> {
    await this.env.DB.prepare(
      'UPDATE devices SET is_active = 0 WHERE tg_user_id = ? AND device_id = ?'
    ).bind(tgUserId, deviceId).run();
  }

  // 解绑用户所有设备
  async unbindAllDevices(tgUserId: string): Promise<void> {
    await this.env.DB.prepare(
      'UPDATE devices SET is_active = 0 WHERE tg_user_id = ?'
    ).bind(tgUserId).run();
  }

  // 检查设备是否被解绑 (通过 KV)
  async isDeviceUnbound(deviceId: string): Promise<boolean> {
    const unbound = await this.env.KV.get(`unbind:${deviceId}`);
    return unbound === '1';
  }
}
