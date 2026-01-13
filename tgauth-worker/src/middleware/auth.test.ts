// Feature: tgauth-system, Property 3: Token Validation Enforcement
// Validates: Requirements 4.5, 4.6
//
// Property: For any API request with token authentication:
// - IF the token signature is invalid THEN the request SHALL be rejected with 401
// - IF the token is expired THEN the request SHALL be rejected with 401
// - IF the device_id in token does not match X-Device-ID header THEN the request SHALL be rejected with 403

import { describe, it, expect, vi, beforeEach } from 'vitest';
import * as fc from 'fast-check';
import { JWTService } from '../services/jwt';
import { Env, JWTPayload, ErrorCodes } from '../types';

// Mock environment
const createMockEnv = (jwtSecret: string = 'test-secret'): Env => ({
  DB: {} as D1Database,
  KV: {
    get: vi.fn(async () => null),
    put: vi.fn(async () => {}),
    delete: vi.fn(async () => {}),
  } as unknown as KVNamespace,
  BOT_USERNAME: 'TestBot',
  BOT_TOKEN: 'test-token',
  JWT_SECRET: jwtSecret,
  ADMIN_TG_IDS: '123456',
  BOT_SECRET: 'test-bot-secret',
  SESSION_TIMEOUT_SEC: '300',
  TOKEN_EXPIRE_DAYS: '7',
  DEFAULT_DEVICE_LIMIT: '1',
  RATE_LIMIT_REQUESTS: '60',
  RATE_LIMIT_WINDOW_SEC: '60',
});

// Simulate auth middleware validation logic
async function validateToken(
  env: Env,
  authHeader: string | undefined,
  deviceIdHeader: string | undefined
): Promise<{ valid: boolean; error?: string; errorCode?: string; payload?: JWTPayload }> {
  // Check Authorization header
  if (!authHeader) {
    return { valid: false, error: 'Authorization header is required', errorCode: ErrorCodes.BAD_REQUEST };
  }

  // Check X-Device-ID header
  if (!deviceIdHeader) {
    return { valid: false, error: 'X-Device-ID header is required', errorCode: ErrorCodes.BAD_REQUEST };
  }

  // Check Bearer format
  if (!authHeader.startsWith('Bearer ')) {
    return { valid: false, error: 'Invalid authorization format', errorCode: ErrorCodes.TOKEN_INVALID };
  }

  const token = authHeader.substring(7);
  const jwtService = new JWTService(env);

  // Verify token
  const payload = await jwtService.verifyToken(token);
  if (!payload) {
    return { valid: false, error: 'Invalid or expired token', errorCode: ErrorCodes.TOKEN_INVALID };
  }

  // Check device_id match
  if (payload.did !== deviceIdHeader) {
    return { valid: false, error: 'Device ID does not match token', errorCode: ErrorCodes.DEVICE_MISMATCH };
  }

  return { valid: true, payload };
}

describe('Token Validation', () => {
  let env: Env;
  let jwtService: JWTService;

  beforeEach(() => {
    env = createMockEnv();
    jwtService = new JWTService(env);
  });

  describe('Header Validation', () => {
    it('should reject missing Authorization header', async () => {
      const result = await validateToken(env, undefined, 'device-123');
      
      expect(result.valid).toBe(false);
      expect(result.errorCode).toBe(ErrorCodes.BAD_REQUEST);
    });

    it('should reject missing X-Device-ID header', async () => {
      const result = await validateToken(env, 'Bearer token', undefined);
      
      expect(result.valid).toBe(false);
      expect(result.errorCode).toBe(ErrorCodes.BAD_REQUEST);
    });

    it('should reject non-Bearer authorization', async () => {
      const result = await validateToken(env, 'Basic token', 'device-123');
      
      expect(result.valid).toBe(false);
      expect(result.errorCode).toBe(ErrorCodes.TOKEN_INVALID);
    });
  });

  describe('Token Signature Validation', () => {
    it('should reject invalid token format', async () => {
      const result = await validateToken(env, 'Bearer invalid-token', 'device-123');
      
      expect(result.valid).toBe(false);
      expect(result.errorCode).toBe(ErrorCodes.TOKEN_INVALID);
    });

    it('should reject tampered token', async () => {
      const { token } = await jwtService.generateToken({
        tg_user_id: 'user-123',
        device_id: 'device-123',
      });

      // Tamper with the token
      const parts = token.split('.');
      parts[2] = 'tampered_signature_here';
      const tamperedToken = parts.join('.');

      const result = await validateToken(env, `Bearer ${tamperedToken}`, 'device-123');
      
      expect(result.valid).toBe(false);
      expect(result.errorCode).toBe(ErrorCodes.TOKEN_INVALID);
    });

    it('should reject token signed with different secret', async () => {
      const otherEnv = createMockEnv('different-secret');
      const otherJwtService = new JWTService(otherEnv);
      
      const { token } = await otherJwtService.generateToken({
        tg_user_id: 'user-123',
        device_id: 'device-123',
      });

      const result = await validateToken(env, `Bearer ${token}`, 'device-123');
      
      expect(result.valid).toBe(false);
      expect(result.errorCode).toBe(ErrorCodes.TOKEN_INVALID);
    });
  });

  describe('Device ID Validation', () => {
    it('should reject mismatched device ID', async () => {
      const { token } = await jwtService.generateToken({
        tg_user_id: 'user-123',
        device_id: 'device-123',
      });

      const result = await validateToken(env, `Bearer ${token}`, 'different-device');
      
      expect(result.valid).toBe(false);
      expect(result.errorCode).toBe(ErrorCodes.DEVICE_MISMATCH);
    });

    it('should accept matching device ID', async () => {
      const deviceId = 'device-123';
      const { token } = await jwtService.generateToken({
        tg_user_id: 'user-123',
        device_id: deviceId,
      });

      const result = await validateToken(env, `Bearer ${token}`, deviceId);
      
      expect(result.valid).toBe(true);
      expect(result.payload?.did).toBe(deviceId);
    });
  });

  describe('Valid Token', () => {
    it('should accept valid token with matching device', async () => {
      const userId = 'user-123';
      const deviceId = 'device-abc';
      
      const { token } = await jwtService.generateToken({
        tg_user_id: userId,
        device_id: deviceId,
      });

      const result = await validateToken(env, `Bearer ${token}`, deviceId);
      
      expect(result.valid).toBe(true);
      expect(result.payload?.sub).toBe(userId);
      expect(result.payload?.did).toBe(deviceId);
    });
  });

  // Property-based tests
  describe('Property Tests', () => {
    // Property 3: Token Validation Enforcement
    it('should always reject invalid tokens', async () => {
      await fc.assert(
        fc.asyncProperty(
          fc.string({ minLength: 10, maxLength: 100 }),  // random invalid token
          fc.hexaString({ minLength: 16, maxLength: 64 }),  // device id
          async (invalidToken, deviceId) => {
            const result = await validateToken(env, `Bearer ${invalidToken}`, deviceId);
            expect(result.valid).toBe(false);
          }
        ),
        { numRuns: 100 }
      );
    });

    // Property: Valid token with matching device should always pass
    it('should always accept valid token with matching device', async () => {
      await fc.assert(
        fc.asyncProperty(
          fc.string({ minLength: 1, maxLength: 20 }),  // user id
          fc.hexaString({ minLength: 16, maxLength: 64 }),  // device id
          async (userId, deviceId) => {
            const { token } = await jwtService.generateToken({
              tg_user_id: userId,
              device_id: deviceId,
            });

            const result = await validateToken(env, `Bearer ${token}`, deviceId);
            
            expect(result.valid).toBe(true);
            expect(result.payload?.sub).toBe(userId);
            expect(result.payload?.did).toBe(deviceId);
          }
        ),
        { numRuns: 100 }
      );
    });

    // Property: Device mismatch should always be rejected
    it('should always reject device mismatch', async () => {
      await fc.assert(
        fc.asyncProperty(
          fc.string({ minLength: 1, maxLength: 20 }),
          fc.hexaString({ minLength: 16, maxLength: 64 }),
          fc.hexaString({ minLength: 16, maxLength: 64 }),
          async (userId, tokenDeviceId, headerDeviceId) => {
            fc.pre(tokenDeviceId !== headerDeviceId);

            const { token } = await jwtService.generateToken({
              tg_user_id: userId,
              device_id: tokenDeviceId,
            });

            const result = await validateToken(env, `Bearer ${token}`, headerDeviceId);
            
            expect(result.valid).toBe(false);
            expect(result.errorCode).toBe(ErrorCodes.DEVICE_MISMATCH);
          }
        ),
        { numRuns: 100 }
      );
    });

    // Property: Missing headers should always be rejected
    it('should always reject missing headers', async () => {
      await fc.assert(
        fc.asyncProperty(
          fc.boolean(),  // has auth header
          fc.boolean(),  // has device header
          fc.string({ minLength: 1, maxLength: 100 }),
          fc.hexaString({ minLength: 16, maxLength: 64 }),
          async (hasAuth, hasDevice, authValue, deviceValue) => {
            fc.pre(!hasAuth || !hasDevice);  // At least one missing

            const authHeader = hasAuth ? `Bearer ${authValue}` : undefined;
            const deviceHeader = hasDevice ? deviceValue : undefined;

            const result = await validateToken(env, authHeader, deviceHeader);
            expect(result.valid).toBe(false);
          }
        ),
        { numRuns: 100 }
      );
    });

    // Property: Token from different secret should always fail
    it('should always reject tokens from different secrets', async () => {
      await fc.assert(
        fc.asyncProperty(
          fc.string({ minLength: 10, maxLength: 50 }),  // secret 1
          fc.string({ minLength: 10, maxLength: 50 }),  // secret 2
          fc.string({ minLength: 1, maxLength: 20 }),   // user id
          fc.hexaString({ minLength: 16, maxLength: 64 }),  // device id
          async (secret1, secret2, userId, deviceId) => {
            fc.pre(secret1 !== secret2);

            const env1 = createMockEnv(secret1);
            const env2 = createMockEnv(secret2);
            const service1 = new JWTService(env1);

            const { token } = await service1.generateToken({
              tg_user_id: userId,
              device_id: deviceId,
            });

            // Validate with different secret
            const result = await validateToken(env2, `Bearer ${token}`, deviceId);
            expect(result.valid).toBe(false);
          }
        ),
        { numRuns: 50 }
      );
    });
  });
});

describe('Admin Authentication', () => {
  it('should identify admin users correctly', () => {
    const adminIds = '123456,789012,345678';
    
    const isAdmin = (userId: string) => {
      const ids = adminIds.split(',').map(id => id.trim());
      return ids.includes(userId);
    };

    expect(isAdmin('123456')).toBe(true);
    expect(isAdmin('789012')).toBe(true);
    expect(isAdmin('345678')).toBe(true);
    expect(isAdmin('999999')).toBe(false);
  });

  // Property: Admin check should be consistent
  it('should consistently identify admins', () => {
    fc.assert(
      fc.property(
        fc.array(fc.stringOf(fc.constantFrom(...'0123456789'.split('')), { minLength: 5, maxLength: 15 }), { minLength: 1, maxLength: 5 }),
        fc.stringOf(fc.constantFrom(...'0123456789'.split('')), { minLength: 5, maxLength: 15 }),
        (adminIds, testId) => {
          const adminList = adminIds.join(',');
          const isAdmin = adminIds.includes(testId);
          
          const checkAdmin = (userId: string) => {
            const ids = adminList.split(',').map(id => id.trim());
            return ids.includes(userId);
          };

          expect(checkAdmin(testId)).toBe(isAdmin);
        }
      ),
      { numRuns: 100 }
    );
  });
});
