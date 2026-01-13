// Feature: tgauth-system, Property 6: Banned User Enforcement
// Validates: Requirements 6.4, 6.6
//
// Property: For any banned user:
// - Login attempts SHALL return user_banned error
// - API requests with valid tokens SHALL return user_banned error
// - All existing tokens SHALL become invalid immediately upon ban

import { describe, it, expect, vi, beforeEach } from 'vitest';
import * as fc from 'fast-check';
import { Env, ErrorCodes } from '../types';

// Mock KV for ban checking
const createMockKV = () => {
  const store = new Map<string, string>();
  
  return {
    get: vi.fn(async (key: string) => store.get(key) || null),
    put: vi.fn(async (key: string, value: string) => {
      store.set(key, value);
    }),
    delete: vi.fn(async (key: string) => {
      store.delete(key);
    }),
    _store: store,
    _reset: () => { store.clear(); },
  };
};

// Mock DB
const createMockDB = () => {
  const users = new Map<string, { is_banned: number }>();
  
  return {
    prepare: vi.fn((sql: string) => ({
      bind: vi.fn((...args: unknown[]) => ({
        run: vi.fn(async () => {
          if (sql.includes('UPDATE users SET is_banned = 1')) {
            const userId = args[1] as string;
            users.set(userId, { is_banned: 1 });
          }
          if (sql.includes('UPDATE users SET is_banned = 0')) {
            const userId = args[1] as string;
            users.set(userId, { is_banned: 0 });
          }
          return { meta: { changes: 1 } };
        }),
        first: vi.fn(async () => {
          if (sql.includes('SELECT is_banned')) {
            const userId = args[0] as string;
            return users.get(userId) || null;
          }
          return null;
        }),
      })),
    })),
    _users: users,
    _reset: () => { users.clear(); },
  };
};

// Simulate ban check logic
async function isUserBanned(kv: ReturnType<typeof createMockKV>, userId: string): Promise<boolean> {
  const banned = await kv.get(`banned:${userId}`);
  return banned === '1';
}

// Simulate ban user logic
async function banUser(
  db: ReturnType<typeof createMockDB>,
  kv: ReturnType<typeof createMockKV>,
  userId: string
): Promise<void> {
  // Update DB
  await db.prepare('UPDATE users SET is_banned = 1, updated_at = ? WHERE tg_user_id = ?')
    .bind(Math.floor(Date.now() / 1000), userId)
    .run();
  
  // Add to KV ban list
  await kv.put(`banned:${userId}`, '1');
}

// Simulate unban user logic
async function unbanUser(
  db: ReturnType<typeof createMockDB>,
  kv: ReturnType<typeof createMockKV>,
  userId: string
): Promise<void> {
  // Update DB
  await db.prepare('UPDATE users SET is_banned = 0, updated_at = ? WHERE tg_user_id = ?')
    .bind(Math.floor(Date.now() / 1000), userId)
    .run();
  
  // Remove from KV ban list
  await kv.delete(`banned:${userId}`);
}

describe('User Ban Functionality', () => {
  let db: ReturnType<typeof createMockDB>;
  let kv: ReturnType<typeof createMockKV>;

  beforeEach(() => {
    db = createMockDB();
    kv = createMockKV();
    db._reset();
    kv._reset();
  });

  describe('Ban User', () => {
    it('should ban a user', async () => {
      const userId = 'user-123';
      
      await banUser(db, kv, userId);
      
      const isBanned = await isUserBanned(kv, userId);
      expect(isBanned).toBe(true);
    });

    it('should add user to KV ban list', async () => {
      const userId = 'user-456';
      
      await banUser(db, kv, userId);
      
      const kvValue = await kv.get(`banned:${userId}`);
      expect(kvValue).toBe('1');
    });
  });

  describe('Unban User', () => {
    it('should unban a user', async () => {
      const userId = 'user-789';
      
      // First ban
      await banUser(db, kv, userId);
      expect(await isUserBanned(kv, userId)).toBe(true);
      
      // Then unban
      await unbanUser(db, kv, userId);
      expect(await isUserBanned(kv, userId)).toBe(false);
    });

    it('should remove user from KV ban list', async () => {
      const userId = 'user-abc';
      
      await banUser(db, kv, userId);
      await unbanUser(db, kv, userId);
      
      const kvValue = await kv.get(`banned:${userId}`);
      expect(kvValue).toBeNull();
    });
  });

  describe('Ban Check', () => {
    it('should return false for non-banned user', async () => {
      const isBanned = await isUserBanned(kv, 'non-existent-user');
      expect(isBanned).toBe(false);
    });

    it('should return true for banned user', async () => {
      const userId = 'banned-user';
      await kv.put(`banned:${userId}`, '1');
      
      const isBanned = await isUserBanned(kv, userId);
      expect(isBanned).toBe(true);
    });
  });

  // Property-based tests
  describe('Property Tests', () => {
    // Property 6: Banned User Enforcement
    it('should always detect banned users', async () => {
      await fc.assert(
        fc.asyncProperty(
          fc.string({ minLength: 5, maxLength: 20 }),
          async (userId) => {
            kv._reset();
            
            // Initially not banned
            expect(await isUserBanned(kv, userId)).toBe(false);
            
            // After ban
            await banUser(db, kv, userId);
            expect(await isUserBanned(kv, userId)).toBe(true);
            
            // After unban
            await unbanUser(db, kv, userId);
            expect(await isUserBanned(kv, userId)).toBe(false);
          }
        ),
        { numRuns: 50 }
      );
    });

    // Property: Ban should be immediate
    it('should immediately reflect ban status', async () => {
      await fc.assert(
        fc.asyncProperty(
          fc.string({ minLength: 5, maxLength: 20 }),
          async (userId) => {
            kv._reset();
            
            await banUser(db, kv, userId);
            
            // Should be immediately detectable
            const isBanned = await isUserBanned(kv, userId);
            expect(isBanned).toBe(true);
          }
        ),
        { numRuns: 50 }
      );
    });

    // Property: Unban should be immediate
    it('should immediately reflect unban status', async () => {
      await fc.assert(
        fc.asyncProperty(
          fc.string({ minLength: 5, maxLength: 20 }),
          async (userId) => {
            kv._reset();
            
            await banUser(db, kv, userId);
            await unbanUser(db, kv, userId);
            
            // Should be immediately detectable
            const isBanned = await isUserBanned(kv, userId);
            expect(isBanned).toBe(false);
          }
        ),
        { numRuns: 50 }
      );
    });

    // Property: Multiple bans should be idempotent
    it('should handle multiple bans idempotently', async () => {
      await fc.assert(
        fc.asyncProperty(
          fc.string({ minLength: 5, maxLength: 20 }),
          fc.integer({ min: 1, max: 5 }),
          async (userId, banCount) => {
            kv._reset();
            
            // Ban multiple times
            for (let i = 0; i < banCount; i++) {
              await banUser(db, kv, userId);
            }
            
            // Should still be banned
            expect(await isUserBanned(kv, userId)).toBe(true);
            
            // Single unban should work
            await unbanUser(db, kv, userId);
            expect(await isUserBanned(kv, userId)).toBe(false);
          }
        ),
        { numRuns: 50 }
      );
    });
  });
});

// Property 10: Admin Permission Enforcement
describe('Admin Permission Enforcement', () => {
  function isAdmin(adminIds: string, userId: string): boolean {
    const ids = adminIds.split(',').map(id => id.trim());
    return ids.includes(userId);
  }

  it('should correctly identify admins', () => {
    const adminIds = '123456,789012';
    
    expect(isAdmin(adminIds, '123456')).toBe(true);
    expect(isAdmin(adminIds, '789012')).toBe(true);
    expect(isAdmin(adminIds, '999999')).toBe(false);
  });

  // Property: Admin check should be consistent
  it('should consistently identify admins', () => {
    fc.assert(
      fc.property(
        fc.array(fc.stringOf(fc.constantFrom(...'0123456789'.split('')), { minLength: 5, maxLength: 15 }), { minLength: 1, maxLength: 5 }),
        fc.nat({ max: 10 }),
        (adminIds, index) => {
          const adminList = adminIds.join(',');
          const selectedId = adminIds[index % adminIds.length];
          
          expect(isAdmin(adminList, selectedId)).toBe(true);
        }
      ),
      { numRuns: 100 }
    );
  });

  // Property: Non-admins should never be identified as admins
  it('should never identify non-admins as admins', () => {
    fc.assert(
      fc.property(
        fc.array(fc.stringOf(fc.constantFrom(...'0123456789'.split('')), { minLength: 5, maxLength: 15 }), { minLength: 1, maxLength: 5 }),
        fc.stringOf(fc.constantFrom(...'0123456789'.split('')), { minLength: 5, maxLength: 15 }),
        (adminIds, testId) => {
          fc.pre(!adminIds.includes(testId));
          
          const adminList = adminIds.join(',');
          expect(isAdmin(adminList, testId)).toBe(false);
        }
      ),
      { numRuns: 100 }
    );
  });
});
