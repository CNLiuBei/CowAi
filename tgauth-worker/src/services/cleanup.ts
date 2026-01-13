// 定时清理服务
import { Env } from '../types';

// 清理配置
const CLEANUP_CONFIG = {
  // 已撤销卡密保留天数
  REVOKED_KEYS_RETENTION_DAYS: 30,
  // 已过期卡密保留天数
  EXPIRED_KEYS_RETENTION_DAYS: 90,
  // 过期会话保留天数
  EXPIRED_SESSIONS_RETENTION_DAYS: 7,
  // 审计日志保留天数
  AUDIT_LOGS_RETENTION_DAYS: 180,
  // 非活跃设备保留天数
  INACTIVE_DEVICES_RETENTION_DAYS: 90,
};

export async function cleanupExpiredData(env: Env): Promise<void> {
  console.log('[Cleanup] Starting scheduled cleanup...');
  const now = Math.floor(Date.now() / 1000);
  
  try {
    // 1. 清理已撤销的卡密（保留30天）
    const revokedCutoff = now - CLEANUP_CONFIG.REVOKED_KEYS_RETENTION_DAYS * 24 * 60 * 60;
    const revokedResult = await env.DB.prepare(
      `DELETE FROM license_keys WHERE status = 'revoked' AND updated_at < ?`
    ).bind(revokedCutoff).run();
    console.log(`[Cleanup] Deleted ${revokedResult.meta.changes} revoked keys`);

    // 2. 清理已过期的卡密（保留90天）
    const expiredCutoff = now - CLEANUP_CONFIG.EXPIRED_KEYS_RETENTION_DAYS * 24 * 60 * 60;
    const expiredResult = await env.DB.prepare(
      `DELETE FROM license_keys WHERE status = 'expired' AND expires_at < ?`
    ).bind(expiredCutoff).run();
    console.log(`[Cleanup] Deleted ${expiredResult.meta.changes} expired keys`);

    // 3. 更新过期但状态未更新的卡密
    const updateExpiredResult = await env.DB.prepare(
      `UPDATE license_keys SET status = 'expired', updated_at = ? 
       WHERE status = 'activated' AND expires_at IS NOT NULL AND expires_at < ?`
    ).bind(now, now).run();
    console.log(`[Cleanup] Updated ${updateExpiredResult.meta.changes} keys to expired status`);

    // 4. 清理过期会话
    const sessionCutoff = now - CLEANUP_CONFIG.EXPIRED_SESSIONS_RETENTION_DAYS * 24 * 60 * 60;
    const sessionResult = await env.DB.prepare(
      `DELETE FROM sessions WHERE (status = 'expired' OR status = 'completed') AND created_at < ?`
    ).bind(sessionCutoff).run();
    console.log(`[Cleanup] Deleted ${sessionResult.meta.changes} old sessions`);

    // 5. 清理旧审计日志
    const auditCutoff = now - CLEANUP_CONFIG.AUDIT_LOGS_RETENTION_DAYS * 24 * 60 * 60;
    const auditResult = await env.DB.prepare(
      `DELETE FROM audit_logs WHERE created_at < ?`
    ).bind(auditCutoff).run();
    console.log(`[Cleanup] Deleted ${auditResult.meta.changes} old audit logs`);

    // 6. 清理非活跃设备（已解绑或长期未活跃）
    const deviceCutoff = now - CLEANUP_CONFIG.INACTIVE_DEVICES_RETENTION_DAYS * 24 * 60 * 60;
    const deviceResult = await env.DB.prepare(
      `DELETE FROM devices WHERE is_active = 0 AND last_seen < ?`
    ).bind(deviceCutoff).run();
    console.log(`[Cleanup] Deleted ${deviceResult.meta.changes} inactive devices`);

    // 7. 清理已删除卡密关联的设备绑定
    const orphanDeviceResult = await env.DB.prepare(
      `DELETE FROM key_devices WHERE key_id NOT IN (SELECT id FROM license_keys)`
    ).run();
    console.log(`[Cleanup] Deleted ${orphanDeviceResult.meta.changes} orphan key-device bindings`);

    console.log('[Cleanup] Scheduled cleanup completed successfully');
  } catch (error) {
    console.error('[Cleanup] Error during cleanup:', error);
    throw error;
  }
}
