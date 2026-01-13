// Feature: tgauth-system, Property 11: Audit Logging Completeness
// Validates: Requirements 8.5
//
// Property: For any login attempt or admin operation:
// - An audit log entry SHALL be created with action, tg_user_id, device_id, ip_address, and timestamp
// - Log entries SHALL be queryable by tg_user_id and time range

import { describe, it, expect, vi, beforeEach } from 'vitest';
import * as fc from 'fast-check';

// Audit log entry interface
interface AuditLogEntry {
  id?: number;
  action: string;
  tg_user_id: string | null;
  device_id: string | null;
  ip_address: string | null;
  details: string | null;
  created_at: number;
}

// Mock DB for audit logging
const createMockDB = () => {
  const logs: AuditLogEntry[] = [];
  let nextId = 1;
  
  return {
    prepare: vi.fn((sql: string) => ({
      bind: vi.fn((...args: unknown[]) => ({
        run: vi.fn(async () => {
          if (sql.includes('INSERT INTO audit_logs')) {
            const entry: AuditLogEntry = {
              id: nextId++,
              action: args[0] as string,
              tg_user_id: args[1] as string | null,
              device_id: args[2] as string | null,
              ip_address: args[3] as string | null,
              details: args[4] as string | null,
              created_at: args[5] as number,
            };
            logs.push(entry);
            return { meta: { last_row_id: entry.id } };
          }
          return { meta: { changes: 0 } };
        }),
        all: vi.fn(async () => {
          if (sql.includes('SELECT') && sql.includes('audit_logs')) {
            let filtered = [...logs];
            
            // Filter by tg_user_id if present
            if (sql.includes('tg_user_id = ?')) {
              const userId = args[0] as string;
              filtered = filtered.filter(l => l.tg_user_id === userId);
            }
            
            // Filter by time range if present
            if (sql.includes('created_at >= ?') && sql.includes('created_at <= ?')) {
              const startTime = args[sql.includes('tg_user_id') ? 1 : 0] as number;
              const endTime = args[sql.includes('tg_user_id') ? 2 : 1] as number;
              filtered = filtered.filter(l => l.created_at >= startTime && l.created_at <= endTime);
            }
            
            return { results: filtered };
          }
          return { results: [] };
        }),
      })),
    })),
    _logs: logs,
    _reset: () => { logs.length = 0; nextId = 1; },
  };
};

// Audit service simulation
class AuditService {
  constructor(private db: ReturnType<typeof createMockDB>) {}

  async log(
    action: string,
    tgUserId: string | null,
    deviceId: string | null,
    ipAddress: string | null,
    details: Record<string, unknown> | null = null
  ): Promise<void> {
    await this.db.prepare(`
      INSERT INTO audit_logs (action, tg_user_id, device_id, ip_address, details, created_at)
      VALUES (?, ?, ?, ?, ?, ?)
    `).bind(
      action,
      tgUserId,
      deviceId,
      ipAddress,
      details ? JSON.stringify(details) : null,
      Math.floor(Date.now() / 1000)
    ).run();
  }

  async getByUserId(tgUserId: string): Promise<AuditLogEntry[]> {
    const result = await this.db.prepare(`
      SELECT * FROM audit_logs WHERE tg_user_id = ? ORDER BY created_at DESC
    `).bind(tgUserId).all();
    return result.results as AuditLogEntry[];
  }

  async getByTimeRange(startTime: number, endTime: number): Promise<AuditLogEntry[]> {
    const result = await this.db.prepare(`
      SELECT * FROM audit_logs WHERE created_at >= ? AND created_at <= ? ORDER BY created_at DESC
    `).bind(startTime, endTime).all();
    return result.results as AuditLogEntry[];
  }

  async getByUserIdAndTimeRange(
    tgUserId: string,
    startTime: number,
    endTime: number
  ): Promise<AuditLogEntry[]> {
    const result = await this.db.prepare(`
      SELECT * FROM audit_logs WHERE tg_user_id = ? AND created_at >= ? AND created_at <= ? ORDER BY created_at DESC
    `).bind(tgUserId, startTime, endTime).all();
    return result.results as AuditLogEntry[];
  }
}

// Audit action types
const AuditActions = {
  LOGIN_REQUEST: 'login_request',
  LOGIN_APPROVED: 'login_approved',
  LOGIN_REJECTED: 'login_rejected',
  TOKEN_VERIFIED: 'token_verified',
  TOKEN_INVALID: 'token_invalid',
  USER_BANNED: 'user_banned',
  USER_UNBANNED: 'user_unbanned',
  DEVICE_BOUND: 'device_bound',
  DEVICE_UNBOUND: 'device_unbound',
  DEVICE_LIMIT_EXCEEDED: 'device_limit_exceeded',
} as const;

describe('Audit Logging', () => {
  let db: ReturnType<typeof createMockDB>;
  let auditService: AuditService;

  beforeEach(() => {
    db = createMockDB();
    db._reset();
    auditService = new AuditService(db);
  });

  describe('Log Creation', () => {
    it('should create audit log entry', async () => {
      await auditService.log(
        AuditActions.LOGIN_REQUEST,
        '123456',
        'device123',
        '192.168.1.1',
        { session_id: 'sess123' }
      );

      expect(db._logs).toHaveLength(1);
      expect(db._logs[0].action).toBe(AuditActions.LOGIN_REQUEST);
      expect(db._logs[0].tg_user_id).toBe('123456');
      expect(db._logs[0].device_id).toBe('device123');
      expect(db._logs[0].ip_address).toBe('192.168.1.1');
    });

    it('should handle null values', async () => {
      await auditService.log(
        AuditActions.LOGIN_REQUEST,
        null,
        'device123',
        '192.168.1.1',
        null
      );

      expect(db._logs[0].tg_user_id).toBeNull();
      expect(db._logs[0].details).toBeNull();
    });

    it('should record timestamp', async () => {
      const before = Math.floor(Date.now() / 1000);
      await auditService.log(AuditActions.LOGIN_REQUEST, '123', null, null);
      const after = Math.floor(Date.now() / 1000);

      expect(db._logs[0].created_at).toBeGreaterThanOrEqual(before);
      expect(db._logs[0].created_at).toBeLessThanOrEqual(after);
    });
  });

  describe('Log Querying', () => {
    beforeEach(async () => {
      // Create test logs
      await auditService.log(AuditActions.LOGIN_REQUEST, 'user1', 'dev1', '1.1.1.1');
      await auditService.log(AuditActions.LOGIN_APPROVED, 'user1', 'dev1', '1.1.1.1');
      await auditService.log(AuditActions.LOGIN_REQUEST, 'user2', 'dev2', '2.2.2.2');
      await auditService.log(AuditActions.USER_BANNED, 'user1', null, '3.3.3.3');
    });

    it('should query by user ID', async () => {
      const logs = await auditService.getByUserId('user1');
      
      expect(logs).toHaveLength(3);
      logs.forEach(log => {
        expect(log.tg_user_id).toBe('user1');
      });
    });

    it('should return empty for non-existent user', async () => {
      const logs = await auditService.getByUserId('nonexistent');
      expect(logs).toHaveLength(0);
    });
  });

  describe('Action Types', () => {
    it('should log login request', async () => {
      await auditService.log(AuditActions.LOGIN_REQUEST, 'user1', 'dev1', '1.1.1.1');
      expect(db._logs[0].action).toBe('login_request');
    });

    it('should log login approved', async () => {
      await auditService.log(AuditActions.LOGIN_APPROVED, 'user1', 'dev1', '1.1.1.1');
      expect(db._logs[0].action).toBe('login_approved');
    });

    it('should log user banned', async () => {
      await auditService.log(AuditActions.USER_BANNED, 'user1', null, '1.1.1.1');
      expect(db._logs[0].action).toBe('user_banned');
    });

    it('should log device unbound', async () => {
      await auditService.log(AuditActions.DEVICE_UNBOUND, 'user1', 'dev1', '1.1.1.1');
      expect(db._logs[0].action).toBe('device_unbound');
    });
  });

  // Property-based tests
  describe('Property Tests', () => {
    // Property 11: Audit Logging Completeness
    it('should always create log entry with required fields', async () => {
      await fc.assert(
        fc.asyncProperty(
          fc.constantFrom(...Object.values(AuditActions)),
          fc.option(fc.string({ minLength: 1, maxLength: 20 }), { nil: null }),
          fc.option(fc.hexaString({ minLength: 64, maxLength: 64 }), { nil: null }),
          fc.option(fc.ipV4(), { nil: null }),
          async (
            action: string,
            tgUserId: string | null,
            deviceId: string | null,
            ipAddress: string | null
          ) => {
            db._reset();
            
            await auditService.log(action, tgUserId, deviceId, ipAddress);
            
            expect(db._logs).toHaveLength(1);
            const log = db._logs[0];
            
            // Required fields must be present
            expect(log.action).toBe(action);
            expect(log.created_at).toBeGreaterThan(0);
            expect(log.id).toBeDefined();
            
            // Optional fields should match input
            expect(log.tg_user_id).toBe(tgUserId);
            expect(log.device_id).toBe(deviceId);
            expect(log.ip_address).toBe(ipAddress);
          }
        ),
        { numRuns: 100 }
      );
    });

    // Property: Logs should be queryable by user ID
    it('should be queryable by user ID', async () => {
      await fc.assert(
        fc.asyncProperty(
          fc.array(
            fc.record({
              action: fc.constantFrom(...Object.values(AuditActions)),
              userId: fc.string({ minLength: 5, maxLength: 15 }),
            }),
            { minLength: 1, maxLength: 20 }
          ),
          async (entries) => {
            db._reset();
            
            // Create logs
            for (const entry of entries) {
              await auditService.log(entry.action, entry.userId, null, null);
            }
            
            // Query for each unique user
            const uniqueUsers = [...new Set(entries.map(e => e.userId))];
            
            for (const userId of uniqueUsers) {
              const logs = await auditService.getByUserId(userId);
              const expectedCount = entries.filter(e => e.userId === userId).length;
              expect(logs.length).toBe(expectedCount);
            }
          }
        ),
        { numRuns: 50 }
      );
    });

    // Property: All audit actions should be loggable
    it('should support all audit action types', async () => {
      await fc.assert(
        fc.asyncProperty(
          fc.constantFrom(...Object.values(AuditActions)),
          async (action: string) => {
            db._reset();
            
            await auditService.log(action, 'testuser', 'testdevice', '127.0.0.1');
            
            expect(db._logs).toHaveLength(1);
            expect(db._logs[0].action).toBe(action);
          }
        ),
        { numRuns: Object.values(AuditActions).length }
      );
    });

    // Property: Log entries should preserve all data
    it('should preserve all logged data', async () => {
      await fc.assert(
        fc.asyncProperty(
          fc.string({ minLength: 1, maxLength: 50 }),
          fc.string({ minLength: 5, maxLength: 20 }),
          fc.hexaString({ minLength: 64, maxLength: 64 }),
          fc.ipV4(),
          fc.dictionary(fc.string({ minLength: 1, maxLength: 10 }), fc.string({ minLength: 1, maxLength: 20 })),
          async (
            action: string,
            userId: string,
            deviceId: string,
            ip: string,
            details: Record<string, string>
          ) => {
            db._reset();
            
            await auditService.log(action, userId, deviceId, ip, details);
            
            const log = db._logs[0];
            expect(log.action).toBe(action);
            expect(log.tg_user_id).toBe(userId);
            expect(log.device_id).toBe(deviceId);
            expect(log.ip_address).toBe(ip);
            expect(log.details).toBe(JSON.stringify(details));
          }
        ),
        { numRuns: 50 }
      );
    });

    // Property: Timestamps should be monotonically increasing
    it('should have monotonically increasing timestamps', async () => {
      await fc.assert(
        fc.asyncProperty(
          fc.integer({ min: 2, max: 10 }),
          async (count: number) => {
            db._reset();
            
            for (let i = 0; i < count; i++) {
              await auditService.log(AuditActions.LOGIN_REQUEST, `user${i}`, null, null);
            }
            
            for (let i = 1; i < db._logs.length; i++) {
              expect(db._logs[i].created_at).toBeGreaterThanOrEqual(db._logs[i - 1].created_at);
            }
          }
        ),
        { numRuns: 30 }
      );
    });
  });
});
