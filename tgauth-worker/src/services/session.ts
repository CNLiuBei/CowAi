import { Env, Session, SessionStatus } from '../types';

export class SessionService {
  private env: Env;

  constructor(env: Env) {
    this.env = env;
  }

  // 创建新会话
  async createSession(deviceId: string): Promise<Session> {
    const sessionId = crypto.randomUUID();
    const now = Math.floor(Date.now() / 1000);
    const timeoutSec = parseInt(this.env.SESSION_TIMEOUT_SEC) || 300;
    const expiresAt = now + timeoutSec;

    const session: Session = {
      session_id: sessionId,
      device_id: deviceId,
      tg_user_id: null,
      status: 'pending',
      created_at: now,
      expires_at: expiresAt,
      approved_at: null,
    };

    // 存储到 D1
    await this.env.DB.prepare(`
      INSERT INTO sessions (session_id, device_id, tg_user_id, status, created_at, expires_at, approved_at)
      VALUES (?, ?, ?, ?, ?, ?, ?)
    `).bind(
      session.session_id,
      session.device_id,
      session.tg_user_id,
      session.status,
      session.created_at,
      session.expires_at,
      session.approved_at
    ).run();

    // 缓存到 KV
    await this.env.KV.put(
      `session:${sessionId}`,
      JSON.stringify(session),
      { expirationTtl: timeoutSec + 60 } // 额外 1 分钟缓冲
    );

    return session;
  }

  // 获取会话
  async getSession(sessionId: string): Promise<Session | null> {
    // 先从 KV 缓存获取
    const cached = await this.env.KV.get(`session:${sessionId}`);
    if (cached) {
      return JSON.parse(cached) as Session;
    }

    // 从 D1 获取
    const session = await this.env.DB.prepare(
      'SELECT * FROM sessions WHERE session_id = ?'
    ).bind(sessionId).first<Session>();

    if (session) {
      // 更新缓存
      const ttl = Math.max(session.expires_at - Math.floor(Date.now() / 1000), 60);
      await this.env.KV.put(
        `session:${sessionId}`,
        JSON.stringify(session),
        { expirationTtl: ttl }
      );
    }

    return session;
  }

  // 批准会话
  async approveSession(sessionId: string, tgUserId: string): Promise<void> {
    const now = Math.floor(Date.now() / 1000);

    // 更新 D1
    await this.env.DB.prepare(`
      UPDATE sessions 
      SET status = 'approved', tg_user_id = ?, approved_at = ?
      WHERE session_id = ?
    `).bind(tgUserId, now, sessionId).run();

    // 更新 KV 缓存
    const session = await this.getSession(sessionId);
    if (session) {
      session.status = 'approved';
      session.tg_user_id = tgUserId;
      session.approved_at = now;
      
      const ttl = Math.max(session.expires_at - now, 60);
      await this.env.KV.put(
        `session:${sessionId}`,
        JSON.stringify(session),
        { expirationTtl: ttl }
      );
    }
  }

  // 拒绝会话
  async rejectSession(sessionId: string): Promise<void> {
    // 更新 D1
    await this.env.DB.prepare(`
      UPDATE sessions SET status = 'rejected' WHERE session_id = ?
    `).bind(sessionId).run();

    // 更新 KV 缓存
    const session = await this.getSession(sessionId);
    if (session) {
      session.status = 'rejected';
      await this.env.KV.put(
        `session:${sessionId}`,
        JSON.stringify(session),
        { expirationTtl: 60 } // 拒绝后只保留 1 分钟
      );
    }
  }

  // 清理过期会话
  async cleanupExpiredSessions(): Promise<number> {
    const now = Math.floor(Date.now() / 1000);
    
    const result = await this.env.DB.prepare(`
      DELETE FROM sessions WHERE expires_at < ? AND status = 'pending'
    `).bind(now).run();

    return result.meta.changes || 0;
  }
}
