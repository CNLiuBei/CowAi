import { Env, AuditLog } from '../types';

export class AuditService {
  private env: Env;

  constructor(env: Env) {
    this.env = env;
  }

  // 记录审计日志
  async log(params: {
    action: string;
    tg_user_id?: string | null;
    device_id?: string | null;
    ip_address?: string | null;
    details?: string | null;
  }): Promise<void> {
    const now = Math.floor(Date.now() / 1000);

    try {
      await this.env.DB.prepare(`
        INSERT INTO audit_logs (action, tg_user_id, device_id, ip_address, details, created_at)
        VALUES (?, ?, ?, ?, ?, ?)
      `).bind(
        params.action,
        params.tg_user_id || null,
        params.device_id || null,
        params.ip_address || null,
        params.details || null,
        now
      ).run();
    } catch (error) {
      // 审计日志失败不应影响主流程
      console.error('Failed to write audit log:', error);
    }
  }

  // 查询用户的审计日志
  async getUserLogs(tgUserId: string, limit: number = 100): Promise<AuditLog[]> {
    const result = await this.env.DB.prepare(`
      SELECT * FROM audit_logs 
      WHERE tg_user_id = ? 
      ORDER BY created_at DESC 
      LIMIT ?
    `).bind(tgUserId, limit).all<AuditLog>();

    return result.results || [];
  }

  // 查询时间范围内的审计日志
  async getLogsByTimeRange(
    startTime: number,
    endTime: number,
    limit: number = 1000
  ): Promise<AuditLog[]> {
    const result = await this.env.DB.prepare(`
      SELECT * FROM audit_logs 
      WHERE created_at >= ? AND created_at <= ?
      ORDER BY created_at DESC 
      LIMIT ?
    `).bind(startTime, endTime, limit).all<AuditLog>();

    return result.results || [];
  }

  // 查询特定操作的审计日志
  async getLogsByAction(action: string, limit: number = 100): Promise<AuditLog[]> {
    const result = await this.env.DB.prepare(`
      SELECT * FROM audit_logs 
      WHERE action = ? 
      ORDER BY created_at DESC 
      LIMIT ?
    `).bind(action, limit).all<AuditLog>();

    return result.results || [];
  }

  // 清理旧的审计日志 (保留最近 N 天)
  async cleanupOldLogs(retentionDays: number = 90): Promise<number> {
    const cutoffTime = Math.floor(Date.now() / 1000) - retentionDays * 24 * 60 * 60;

    const result = await this.env.DB.prepare(
      'DELETE FROM audit_logs WHERE created_at < ?'
    ).bind(cutoffTime).run();

    return result.meta.changes || 0;
  }
}
