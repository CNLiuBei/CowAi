// Feature: tgauth-system, Property 9: Token Invalidation on Unbind
// Validates: Requirements 10.3
//
// Property: For any device unbind operation:
// - Tokens associated with the unbound device SHALL become invalid
// - Subsequent API requests with those tokens SHALL return 401 Unauthorized

import { describe, it, expect, vi, beforeEach } from 'vitest';
import * as fc from 'fast-check';

// Mock KV for token blacklist
const createMockKV = () => {
  const store = new Map<string, { value: string; expiration?: number }>();
  
  return {
    get: vi.fn(async (key: string) => {
      const item = store.get(key);
      if (!item) return null;
      if (item.expiration && Date.now() > item.expiration) {
        store.delete(key);
        return null;
      }
      return item.value;
    }),
    put: vi.fn(async (key: string, value: string, options?: { expirationTtl?: number }) => {
      const expiration = options?.expirationTtl 
        ? Date.now() + options.expirationTtl * 1000 
        : undefined;
      store.set(key, { value, expiration });
    }),
    delete: vi.fn(async (key: string) => {
      store.delete(key);
    }),
    _store: store,
    _reset: () => { store.clear(); },
  };
};

// Mock DB for devices
const createMockDB = () => {
  const devices: Array<{
    device_id: string;
    tg_user_id: string;
    is_active: number;
  }> = [];
  
  return {
    prepare: vi.fn((sql: string) => ({
      bind: vi.fn((...args: unknown[]) => ({
        run: vi.fn(async () => {
          if (sql.includes('UPDATE devices SET is_active = 0')) {
            const tgUserId = args[0] as string;
            const deviceId = args[1] as string | undefined;
            
            devices.forEach(d => {
              if (d.tg_user_id === tgUserId && (!deviceId || d.device_id === deviceId)) {
                d.is_active = 0;
              }
            });
            return { meta: { changes: 1 } };
          }
          if (sql.includes('INSERT INTO devices')) {
            devices.push({
              device_id: args[0] as string,
              tg_user_id: args[1] as string,
              is_active: 1,
            });
            return { meta: { changes: 1 } };
          }
          return { meta: { changes: 0 } };
        }),
        all: vi.fn(async () => {
          if (sql.includes('SELECT device_id FROM devices')) {
            const tgUserId = args[0] as string;
            return {
              results: devices.filter(d => d.tg_user_id === tgUserId && d.is_active === 1),
            };
          }
          return { results: [] };
        }),
        first: vi.fn(async () => {
          if (sql.includes('SELECT') && sql.includes('devices')) {
            const deviceId = args[0] as string;
            const tgUserId = args[1] as string;
            return devices.find(d => d.device_id === deviceId && d.tg_user_id === tgUserId) || null;
          }
          return null;
        }),
      })),
    })),
    _devices: devices,
    _reset: () => { devices.length = 0; },
    _addDevice: (deviceId: string, tgUserId: string) => {
      devices.push({ device_id: deviceId, tg_user_id: tgUserId, is_active: 1 });
    },
  };
};

// Token structure
interface TokenPayload {
  sub: string;  // tg_user_id
  did: string;  // device_id
  iat: number;
  exp: number;
  jti: string;
}

// Unbind service simulation
class UnbindService {
  constructor(
    private db: ReturnType<typeof createMockDB>,
    private kv: ReturnType<typeof createMockKV>
  ) {}

  // Unbind a specific device
  async unbindDevice(tgUserId: string, deviceId: string): Promise<void> {
    // Mark device as inactive
    await this.db.prepare(
      'UPDATE devices SET is_active = 0 WHERE tg_user_id = ? AND device_id = ?'
    ).bind(tgUserId, deviceId).run();

    // Add device to unbind blacklist (tokens for this device become invalid)
    // TTL: 7 days (longer than token expiry)
    await this.kv.put(`unbind:${deviceId}`, '1', { expirationTtl: 60 * 60 * 24 * 7 });
  }

  // Unbind all devices for a user
  async unbindAllDevices(tgUserId: string): Promise<string[]> {
    // Get all active devices
    const result = await this.db.prepare(
      'SELECT device_id FROM devices WHERE tg_user_id = ? AND is_active = 1'
    ).bind(tgUserId).all();

    const deviceIds: string[] = [];

    // Mark all devices as inactive
    await this.db.prepare(
      'UPDATE devices SET is_active = 0 WHERE tg_user_id = ?'
    ).bind(tgUserId).run();

    // Add all devices to unbind blacklist
    if (result.results) {
      for (const device of result.results as Array<{ device_id: string }>) {
        await this.kv.put(`unbind:${device.device_id}`, '1', { expirationTtl: 60 * 60 * 24 * 7 });
        deviceIds.push(device.device_id);
      }
    }

    return deviceIds;
  }

  // Check if a device has been unbound (token should be invalid)
  async isDeviceUnbound(deviceId: string): Promise<boolean> {
    const unbound = await this.kv.get(`unbind:${deviceId}`);
    return unbound === '1';
  }
}

// Token validation with unbind check
async function validateTokenWithUnbindCheck(
  kv: ReturnType<typeof createMockKV>,
  token: TokenPayload
): Promise<{ valid: boolean; error?: string }> {
  // Check if device has been unbound
  const unbound = await kv.get(`unbind:${token.did}`);
  if (unbound === '1') {
    return { valid: false, error: 'device_unbound' };
  }

  // Check token expiry
  if (token.exp < Math.floor(Date.now() / 1000)) {
    return { valid: false, error: 'token_expired' };
  }

  return { valid: true };
}

describe('Token Invalidation on Unbind', () => {
  let db: ReturnType<typeof createMockDB>;
  let kv: ReturnType<typeof createMockKV>;
  let unbindService: UnbindService;

  beforeEach(() => {
    db = createMockDB();
    kv = createMockKV();
    db._reset();
    kv._reset();
    unbindService = new UnbindService(db, kv);
  });

  describe('Single Device Unbind', () => {
    it('should mark device as unbound', async () => {
      db._addDevice('device123', 'user456');
      
      await unbindService.unbindDevice('user456', 'device123');
      
      const isUnbound = await unbindService.isDeviceUnbound('device123');
      expect(isUnbound).toBe(true);
    });

    it('should invalidate token after unbind', async () => {
      const deviceId = 'device789';
      const token: TokenPayload = {
        sub: 'user123',
        did: deviceId,
        iat: Math.floor(Date.now() / 1000),
        exp: Math.floor(Date.now() / 1000) + 3600,
        jti: 'token-id-123',
      };

      db._addDevice(deviceId, 'user123');

      // Token should be valid before unbind
      let result = await validateTokenWithUnbindCheck(kv, token);
      expect(result.valid).toBe(true);

      // Unbind device
      await unbindService.unbindDevice('user123', deviceId);

      // Token should be invalid after unbind
      result = await validateTokenWithUnbindCheck(kv, token);
      expect(result.valid).toBe(false);
      expect(result.error).toBe('device_unbound');
    });
  });

  describe('All Devices Unbind', () => {
    it('should unbind all devices for user', async () => {
      db._addDevice('device1', 'user1');
      db._addDevice('device2', 'user1');
      db._addDevice('device3', 'user1');

      const unboundDevices = await unbindService.unbindAllDevices('user1');

      expect(unboundDevices).toHaveLength(3);
      
      for (const deviceId of unboundDevices) {
        const isUnbound = await unbindService.isDeviceUnbound(deviceId);
        expect(isUnbound).toBe(true);
      }
    });

    it('should invalidate all tokens after unbind all', async () => {
      const devices = ['devA', 'devB', 'devC'];
      const tokens: TokenPayload[] = devices.map(did => ({
        sub: 'userX',
        did,
        iat: Math.floor(Date.now() / 1000),
        exp: Math.floor(Date.now() / 1000) + 3600,
        jti: `token-${did}`,
      }));

      devices.forEach(d => db._addDevice(d, 'userX'));

      // All tokens should be valid before unbind
      for (const token of tokens) {
        const result = await validateTokenWithUnbindCheck(kv, token);
        expect(result.valid).toBe(true);
      }

      // Unbind all devices
      await unbindService.unbindAllDevices('userX');

      // All tokens should be invalid after unbind
      for (const token of tokens) {
        const result = await validateTokenWithUnbindCheck(kv, token);
        expect(result.valid).toBe(false);
        expect(result.error).toBe('device_unbound');
      }
    });
  });

  describe('Unbind Isolation', () => {
    it('should not affect other users devices', async () => {
      db._addDevice('device1', 'user1');
      db._addDevice('device2', 'user2');

      await unbindService.unbindDevice('user1', 'device1');

      // User1's device should be unbound
      expect(await unbindService.isDeviceUnbound('device1')).toBe(true);
      
      // User2's device should not be affected
      expect(await unbindService.isDeviceUnbound('device2')).toBe(false);
    });

    it('should not affect other devices of same user', async () => {
      db._addDevice('deviceA', 'user1');
      db._addDevice('deviceB', 'user1');

      await unbindService.unbindDevice('user1', 'deviceA');

      expect(await unbindService.isDeviceUnbound('deviceA')).toBe(true);
      expect(await unbindService.isDeviceUnbound('deviceB')).toBe(false);
    });
  });

  // Property-based tests
  describe('Property Tests', () => {
    // Property 9: Token Invalidation on Unbind
    it('should always invalidate tokens after device unbind', async () => {
      await fc.assert(
        fc.asyncProperty(
          fc.hexaString({ minLength: 64, maxLength: 64 }),
          fc.string({ minLength: 5, maxLength: 20 }),
          async (deviceId: string, userId: string) => {
            db._reset();
            kv._reset();
            unbindService = new UnbindService(db, kv);

            db._addDevice(deviceId, userId);

            const token: TokenPayload = {
              sub: userId,
              did: deviceId,
              iat: Math.floor(Date.now() / 1000),
              exp: Math.floor(Date.now() / 1000) + 3600,
              jti: `token-${deviceId}`,
            };

            // Valid before unbind
            let result = await validateTokenWithUnbindCheck(kv, token);
            expect(result.valid).toBe(true);

            // Unbind
            await unbindService.unbindDevice(userId, deviceId);

            // Invalid after unbind
            result = await validateTokenWithUnbindCheck(kv, token);
            expect(result.valid).toBe(false);
            expect(result.error).toBe('device_unbound');
          }
        ),
        { numRuns: 50 }
      );
    });

    // Property: Unbind should be immediate
    it('should immediately invalidate tokens', async () => {
      await fc.assert(
        fc.asyncProperty(
          fc.hexaString({ minLength: 64, maxLength: 64 }),
          fc.string({ minLength: 5, maxLength: 20 }),
          async (deviceId: string, userId: string) => {
            db._reset();
            kv._reset();
            unbindService = new UnbindService(db, kv);

            db._addDevice(deviceId, userId);

            await unbindService.unbindDevice(userId, deviceId);

            // Should be immediately detectable
            const isUnbound = await unbindService.isDeviceUnbound(deviceId);
            expect(isUnbound).toBe(true);
          }
        ),
        { numRuns: 50 }
      );
    });

    // Property: Unbind all should invalidate all user tokens
    it('should invalidate all tokens when unbinding all devices', async () => {
      await fc.assert(
        fc.asyncProperty(
          fc.string({ minLength: 5, maxLength: 20 }),
          fc.array(fc.hexaString({ minLength: 64, maxLength: 64 }), { minLength: 1, maxLength: 5 }),
          async (userId: string, deviceIds: string[]) => {
            db._reset();
            kv._reset();
            unbindService = new UnbindService(db, kv);

            // Add all devices
            const uniqueDevices = [...new Set(deviceIds)];
            uniqueDevices.forEach(d => db._addDevice(d, userId));

            // Unbind all
            await unbindService.unbindAllDevices(userId);

            // All should be unbound
            for (const deviceId of uniqueDevices) {
              const isUnbound = await unbindService.isDeviceUnbound(deviceId);
              expect(isUnbound).toBe(true);
            }
          }
        ),
        { numRuns: 50 }
      );
    });

    // Property: Unbind should not affect unrelated devices
    it('should not affect unrelated devices', async () => {
      await fc.assert(
        fc.asyncProperty(
          fc.string({ minLength: 5, maxLength: 20 }),
          fc.string({ minLength: 5, maxLength: 20 }),
          fc.hexaString({ minLength: 64, maxLength: 64 }),
          fc.hexaString({ minLength: 64, maxLength: 64 }),
          async (user1: string, user2: string, device1: string, device2: string) => {
            fc.pre(user1 !== user2);
            fc.pre(device1 !== device2);

            db._reset();
            kv._reset();
            unbindService = new UnbindService(db, kv);

            db._addDevice(device1, user1);
            db._addDevice(device2, user2);

            // Unbind user1's device
            await unbindService.unbindDevice(user1, device1);

            // User1's device should be unbound
            expect(await unbindService.isDeviceUnbound(device1)).toBe(true);
            
            // User2's device should NOT be unbound
            expect(await unbindService.isDeviceUnbound(device2)).toBe(false);
          }
        ),
        { numRuns: 50 }
      );
    });

    // Property: Multiple unbinds should be idempotent
    it('should handle multiple unbinds idempotently', async () => {
      await fc.assert(
        fc.asyncProperty(
          fc.hexaString({ minLength: 64, maxLength: 64 }),
          fc.string({ minLength: 5, maxLength: 20 }),
          fc.integer({ min: 1, max: 5 }),
          async (deviceId: string, userId: string, unbindCount: number) => {
            db._reset();
            kv._reset();
            unbindService = new UnbindService(db, kv);

            db._addDevice(deviceId, userId);

            // Unbind multiple times
            for (let i = 0; i < unbindCount; i++) {
              await unbindService.unbindDevice(userId, deviceId);
            }

            // Should still be unbound
            expect(await unbindService.isDeviceUnbound(deviceId)).toBe(true);
          }
        ),
        { numRuns: 50 }
      );
    });
  });
});
