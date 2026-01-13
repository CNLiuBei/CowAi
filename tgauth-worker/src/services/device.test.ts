// Feature: tgauth-system, Property 5: Device Binding Enforcement
// Validates: Requirements 5.2, 5.3, 5.4, 5.5
//
// Property: For any user with device_limit = N:
// - The user SHALL be able to bind up to N devices
// - Attempting to bind device N+1 SHALL return device_limit_exceeded error
// - Each binding SHALL record tg_user_id, device_id, and bound_at timestamp

import { describe, it, expect, beforeEach, vi } from 'vitest';
import * as fc from 'fast-check';
import { DeviceService } from './device';
import { Env, Device } from '../types';

// Mock D1 Database with device tracking
const createMockDB = () => {
  const devices: Device[] = [];
  
  return {
    prepare: vi.fn((sql: string) => ({
      bind: vi.fn((...args: unknown[]) => ({
        run: vi.fn(async () => {
          if (sql.includes('INSERT INTO devices')) {
            const device: Device = {
              id: devices.length + 1,
              device_id: args[0] as string,
              tg_user_id: args[1] as string,
              device_info: args[2] as string | null,
              bound_at: args[3] as number,
              last_seen: args[4] as number,
              is_active: 1,
            };
            
            // Check for existing device
            const existing = devices.find(
              d => d.device_id === device.device_id && d.tg_user_id === device.tg_user_id
            );
            
            if (existing) {
              existing.last_seen = device.last_seen;
              existing.is_active = 1;
            } else {
              devices.push(device);
            }
            
            return { meta: { changes: 1 } };
          }
          
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
          
          if (sql.includes('UPDATE devices SET last_seen')) {
            return { meta: { changes: 1 } };
          }
          
          return { meta: { changes: 0 } };
        }),
        first: vi.fn(async () => {
          if (sql.includes('SELECT id FROM devices WHERE tg_user_id')) {
            const tgUserId = args[0] as string;
            const deviceId = args[1] as string;
            const device = devices.find(
              d => d.tg_user_id === tgUserId && d.device_id === deviceId && d.is_active === 1
            );
            return device ? { id: device.id } : null;
          }
          
          if (sql.includes('SELECT COUNT')) {
            const tgUserId = args[0] as string;
            const count = devices.filter(
              d => d.tg_user_id === tgUserId && d.is_active === 1
            ).length;
            return { count };
          }
          
          return null;
        }),
        all: vi.fn(async () => {
          if (sql.includes('SELECT * FROM devices WHERE tg_user_id')) {
            const tgUserId = args[0] as string;
            let userDevices = devices.filter(d => d.tg_user_id === tgUserId);
            
            // Check if query filters by is_active
            if (sql.includes('is_active = 1')) {
              userDevices = userDevices.filter(d => d.is_active === 1);
            }
            
            return { results: userDevices };
          }
          
          return { results: [] };
        }),
      })),
    })),
    _devices: devices,
    _reset: () => { devices.length = 0; },
  };
};

// Mock KV
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

const createMockEnv = (): Env & { _db: ReturnType<typeof createMockDB>; _kv: ReturnType<typeof createMockKV> } => {
  const db = createMockDB();
  const kv = createMockKV();
  
  return {
    DB: db as unknown as D1Database,
    KV: kv as unknown as KVNamespace,
    BOT_USERNAME: 'TestBot',
    BOT_TOKEN: 'test-token',
    JWT_SECRET: 'test-secret',
    ADMIN_TG_IDS: '123456',
    BOT_SECRET: 'test-bot-secret',
    SESSION_TIMEOUT_SEC: '300',
    TOKEN_EXPIRE_DAYS: '7',
    DEFAULT_DEVICE_LIMIT: '1',
    RATE_LIMIT_REQUESTS: '60',
    RATE_LIMIT_WINDOW_SEC: '60',
    _db: db,
    _kv: kv,
  };
};

describe('Device Service', () => {
  let deviceService: DeviceService;
  let env: ReturnType<typeof createMockEnv>;

  beforeEach(() => {
    env = createMockEnv();
    env._db._reset();
    env._kv._reset();
    deviceService = new DeviceService(env);
  });

  describe('Device Binding', () => {
    it('should bind a device to a user', async () => {
      await deviceService.bindDevice('user-123', 'device-abc');
      
      const devices = await deviceService.getUserDevices('user-123');
      expect(devices.length).toBe(1);
      expect(devices[0].device_id).toBe('device-abc');
      expect(devices[0].tg_user_id).toBe('user-123');
    });

    it('should record bound_at timestamp', async () => {
      const beforeBind = Math.floor(Date.now() / 1000);
      await deviceService.bindDevice('user-123', 'device-abc');
      const afterBind = Math.floor(Date.now() / 1000);
      
      const devices = await deviceService.getUserDevices('user-123');
      expect(devices[0].bound_at).toBeGreaterThanOrEqual(beforeBind);
      expect(devices[0].bound_at).toBeLessThanOrEqual(afterBind + 1);
    });

    it('should update existing device binding', async () => {
      await deviceService.bindDevice('user-123', 'device-abc');
      await deviceService.bindDevice('user-123', 'device-abc');
      
      const devices = await deviceService.getUserDevices('user-123');
      // Should still be 1 device (updated, not duplicated)
      expect(devices.length).toBe(1);
    });
  });

  describe('Device Limit Check', () => {
    it('should allow binding when under limit', async () => {
      const canBind = await deviceService.canBindDevice('user-123', 'device-abc', 2);
      expect(canBind).toBe(true);
    });

    it('should allow re-binding existing device', async () => {
      await deviceService.bindDevice('user-123', 'device-abc');
      
      const canBind = await deviceService.canBindDevice('user-123', 'device-abc', 1);
      expect(canBind).toBe(true);
    });

    it('should reject when at limit with new device', async () => {
      await deviceService.bindDevice('user-123', 'device-abc');
      
      const canBind = await deviceService.canBindDevice('user-123', 'device-xyz', 1);
      expect(canBind).toBe(false);
    });

    it('should allow binding up to limit', async () => {
      const limit = 3;
      
      // Bind devices up to limit
      for (let i = 0; i < limit; i++) {
        const canBind = await deviceService.canBindDevice('user-123', `device-${i}`, limit);
        expect(canBind).toBe(true);
        await deviceService.bindDevice('user-123', `device-${i}`);
      }
      
      // Should reject the next one
      const canBindExtra = await deviceService.canBindDevice('user-123', 'device-extra', limit);
      expect(canBindExtra).toBe(false);
    });
  });

  describe('Device Unbinding', () => {
    it('should unbind a specific device', async () => {
      await deviceService.bindDevice('user-123', 'device-abc');
      await deviceService.bindDevice('user-123', 'device-xyz');
      
      await deviceService.unbindDevice('user-123', 'device-abc');
      
      const activeDevices = await deviceService.getActiveDevices('user-123');
      expect(activeDevices.length).toBe(1);
      expect(activeDevices[0].device_id).toBe('device-xyz');
    });

    it('should unbind all devices', async () => {
      await deviceService.bindDevice('user-123', 'device-abc');
      await deviceService.bindDevice('user-123', 'device-xyz');
      
      await deviceService.unbindAllDevices('user-123');
      
      const activeDevices = await deviceService.getActiveDevices('user-123');
      expect(activeDevices.length).toBe(0);
    });
  });

  describe('Device Unbound Check', () => {
    it('should detect unbound device via KV', async () => {
      await env._kv.put('unbind:device-abc', '1');
      
      const isUnbound = await deviceService.isDeviceUnbound('device-abc');
      expect(isUnbound).toBe(true);
    });

    it('should return false for active device', async () => {
      const isUnbound = await deviceService.isDeviceUnbound('device-xyz');
      expect(isUnbound).toBe(false);
    });
  });

  // Property-based tests
  describe('Property Tests', () => {
    // Property 5: Device Binding Enforcement
    it('should enforce device limit correctly', async () => {
      await fc.assert(
        fc.asyncProperty(
          fc.integer({ min: 1, max: 5 }),  // device limit
          fc.integer({ min: 1, max: 10 }), // number of devices to try
          async (limit, numDevices) => {
            // Reset state
            env._db._reset();
            deviceService = new DeviceService(env);
            
            const userId = 'test-user';
            let boundCount = 0;
            
            for (let i = 0; i < numDevices; i++) {
              const deviceId = `device-${i}`;
              const canBind = await deviceService.canBindDevice(userId, deviceId, limit);
              
              if (canBind) {
                await deviceService.bindDevice(userId, deviceId);
                boundCount++;
              }
            }
            
            // Should never exceed limit
            expect(boundCount).toBeLessThanOrEqual(limit);
            
            // If we tried more than limit, we should have exactly limit bound
            if (numDevices >= limit) {
              expect(boundCount).toBe(limit);
            }
          }
        ),
        { numRuns: 50 }
      );
    });

    // Property: Re-binding same device should always succeed
    it('should always allow re-binding existing device', async () => {
      await fc.assert(
        fc.asyncProperty(
          fc.string({ minLength: 5, maxLength: 20 }),
          fc.hexaString({ minLength: 16, maxLength: 64 }),
          fc.integer({ min: 1, max: 5 }),
          async (userId, deviceId, limit) => {
            env._db._reset();
            deviceService = new DeviceService(env);
            
            // First bind
            await deviceService.bindDevice(userId, deviceId);
            
            // Re-bind should always succeed
            const canRebind = await deviceService.canBindDevice(userId, deviceId, limit);
            expect(canRebind).toBe(true);
          }
        ),
        { numRuns: 50 }
      );
    });

    // Property: Binding should record all required fields
    it('should record all required fields on binding', async () => {
      await fc.assert(
        fc.asyncProperty(
          fc.string({ minLength: 5, maxLength: 20 }),
          fc.hexaString({ minLength: 16, maxLength: 64 }),
          async (userId, deviceId) => {
            env._db._reset();
            deviceService = new DeviceService(env);
            
            const beforeBind = Math.floor(Date.now() / 1000);
            await deviceService.bindDevice(userId, deviceId);
            const afterBind = Math.floor(Date.now() / 1000);
            
            const devices = await deviceService.getUserDevices(userId);
            expect(devices.length).toBe(1);
            
            const device = devices[0];
            expect(device.tg_user_id).toBe(userId);
            expect(device.device_id).toBe(deviceId);
            expect(device.bound_at).toBeGreaterThanOrEqual(beforeBind);
            expect(device.bound_at).toBeLessThanOrEqual(afterBind + 1);
            expect(device.is_active).toBe(1);
          }
        ),
        { numRuns: 50 }
      );
    });

    // Property: Unbinding should make device inactive
    it('should make device inactive after unbinding', async () => {
      await fc.assert(
        fc.asyncProperty(
          fc.string({ minLength: 5, maxLength: 20 }),
          fc.hexaString({ minLength: 16, maxLength: 64 }),
          async (userId, deviceId) => {
            env._db._reset();
            deviceService = new DeviceService(env);
            
            await deviceService.bindDevice(userId, deviceId);
            await deviceService.unbindDevice(userId, deviceId);
            
            const activeDevices = await deviceService.getActiveDevices(userId);
            const unboundDevice = activeDevices.find(d => d.device_id === deviceId);
            
            expect(unboundDevice).toBeUndefined();
          }
        ),
        { numRuns: 50 }
      );
    });

    // Property: After unbinding, new device can be bound
    it('should allow new device after unbinding', async () => {
      await fc.assert(
        fc.asyncProperty(
          fc.string({ minLength: 5, maxLength: 20 }),
          fc.hexaString({ minLength: 16, maxLength: 64 }),
          fc.hexaString({ minLength: 16, maxLength: 64 }),
          async (userId, deviceId1, deviceId2) => {
            fc.pre(deviceId1 !== deviceId2);
            
            env._db._reset();
            deviceService = new DeviceService(env);
            
            // Bind first device (limit = 1)
            await deviceService.bindDevice(userId, deviceId1);
            
            // Can't bind second device
            let canBind = await deviceService.canBindDevice(userId, deviceId2, 1);
            expect(canBind).toBe(false);
            
            // Unbind first device
            await deviceService.unbindDevice(userId, deviceId1);
            
            // Now can bind second device
            canBind = await deviceService.canBindDevice(userId, deviceId2, 1);
            expect(canBind).toBe(true);
          }
        ),
        { numRuns: 50 }
      );
    });
  });
});
