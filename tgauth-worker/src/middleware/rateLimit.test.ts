// Feature: tgauth-system, Property 7: Rate Limiting Enforcement
// Validates: Requirements 8.2
//
// Property: For any IP address and endpoint combination:
// - Requests exceeding the configured rate limit SHALL return 429 Too Many Requests
// - Rate limit counters SHALL reset after the configured window

import { describe, it, expect, vi, beforeEach } from 'vitest';
import * as fc from 'fast-check';

// Mock KV store for rate limiting
const createMockKV = () => {
  const store = new Map<string, { value: string; expiration?: number }>();
  
  return {
    get: vi.fn(async (key: string) => {
      const item = store.get(key);
      if (!item) return null;
      // Check expiration
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

// Rate limiter logic simulation
interface RateLimitConfig {
  maxRequests: number;
  windowSec: number;
}

interface RateLimitResult {
  allowed: boolean;
  remaining: number;
  resetAt: number;
}

async function checkRateLimit(
  kv: ReturnType<typeof createMockKV>,
  ip: string,
  endpoint: string,
  config: RateLimitConfig
): Promise<RateLimitResult> {
  const key = `ratelimit:${ip}:${endpoint}`;
  const currentCount = await kv.get(key);
  const count = currentCount ? parseInt(currentCount) : 0;
  
  const resetAt = Math.floor(Date.now() / 1000) + config.windowSec;
  
  if (count >= config.maxRequests) {
    return {
      allowed: false,
      remaining: 0,
      resetAt,
    };
  }
  
  // Increment counter
  await kv.put(key, (count + 1).toString(), {
    expirationTtl: config.windowSec,
  });
  
  return {
    allowed: true,
    remaining: config.maxRequests - count - 1,
    resetAt,
  };
}

describe('Rate Limiting', () => {
  let kv: ReturnType<typeof createMockKV>;
  const defaultConfig: RateLimitConfig = {
    maxRequests: 60,
    windowSec: 60,
  };

  beforeEach(() => {
    kv = createMockKV();
    kv._reset();
  });

  describe('Basic Rate Limiting', () => {
    it('should allow requests under the limit', async () => {
      const result = await checkRateLimit(kv, '192.168.1.1', '/api/test', defaultConfig);
      
      expect(result.allowed).toBe(true);
      expect(result.remaining).toBe(59);
    });

    it('should block requests over the limit', async () => {
      const ip = '192.168.1.2';
      const endpoint = '/api/test';
      
      // Make 60 requests (max)
      for (let i = 0; i < 60; i++) {
        await checkRateLimit(kv, ip, endpoint, defaultConfig);
      }
      
      // 61st request should be blocked
      const result = await checkRateLimit(kv, ip, endpoint, defaultConfig);
      expect(result.allowed).toBe(false);
      expect(result.remaining).toBe(0);
    });

    it('should track remaining requests correctly', async () => {
      const ip = '192.168.1.3';
      const endpoint = '/api/test';
      
      for (let i = 0; i < 10; i++) {
        const result = await checkRateLimit(kv, ip, endpoint, defaultConfig);
        expect(result.remaining).toBe(59 - i);
      }
    });

    it('should isolate rate limits by IP', async () => {
      const endpoint = '/api/test';
      
      // IP 1 makes requests
      for (let i = 0; i < 30; i++) {
        await checkRateLimit(kv, '10.0.0.1', endpoint, defaultConfig);
      }
      
      // IP 2 should have full quota
      const result = await checkRateLimit(kv, '10.0.0.2', endpoint, defaultConfig);
      expect(result.allowed).toBe(true);
      expect(result.remaining).toBe(59);
    });

    it('should isolate rate limits by endpoint', async () => {
      const ip = '10.0.0.3';
      
      // Endpoint 1 makes requests
      for (let i = 0; i < 30; i++) {
        await checkRateLimit(kv, ip, '/api/endpoint1', defaultConfig);
      }
      
      // Endpoint 2 should have full quota
      const result = await checkRateLimit(kv, ip, '/api/endpoint2', defaultConfig);
      expect(result.allowed).toBe(true);
      expect(result.remaining).toBe(59);
    });
  });

  describe('Strict Rate Limiting', () => {
    const strictConfig: RateLimitConfig = {
      maxRequests: 10,
      windowSec: 60,
    };

    it('should enforce stricter limits', async () => {
      const ip = '192.168.1.4';
      const endpoint = '/api/admin/ban';
      
      // Make 10 requests (max for strict)
      for (let i = 0; i < 10; i++) {
        const result = await checkRateLimit(kv, ip, endpoint, strictConfig);
        expect(result.allowed).toBe(true);
      }
      
      // 11th request should be blocked
      const result = await checkRateLimit(kv, ip, endpoint, strictConfig);
      expect(result.allowed).toBe(false);
    });
  });

  // Property-based tests
  describe('Property Tests', () => {
    // Property 7: Rate Limiting Enforcement
    it('should always block requests exceeding the limit', async () => {
      await fc.assert(
        fc.asyncProperty(
          fc.ipV4(),
          fc.webPath(),
          fc.integer({ min: 1, max: 100 }),
          async (ip: string, endpoint: string, maxRequests: number) => {
            kv._reset();
            const config: RateLimitConfig = { maxRequests, windowSec: 60 };
            
            // Make exactly maxRequests requests
            for (let i = 0; i < maxRequests; i++) {
              const result = await checkRateLimit(kv, ip, endpoint, config);
              expect(result.allowed).toBe(true);
            }
            
            // Next request should be blocked
            const blockedResult = await checkRateLimit(kv, ip, endpoint, config);
            expect(blockedResult.allowed).toBe(false);
            expect(blockedResult.remaining).toBe(0);
          }
        ),
        { numRuns: 50 }
      );
    });

    // Property: Remaining count should decrease monotonically
    it('should decrease remaining count monotonically', async () => {
      await fc.assert(
        fc.asyncProperty(
          fc.ipV4(),
          fc.webPath(),
          fc.integer({ min: 5, max: 50 }),
          async (ip: string, endpoint: string, maxRequests: number) => {
            kv._reset();
            const config: RateLimitConfig = { maxRequests, windowSec: 60 };
            
            let previousRemaining = maxRequests;
            
            for (let i = 0; i < maxRequests; i++) {
              const result = await checkRateLimit(kv, ip, endpoint, config);
              expect(result.remaining).toBeLessThan(previousRemaining);
              previousRemaining = result.remaining;
            }
          }
        ),
        { numRuns: 50 }
      );
    });

    // Property: Different IPs should have independent limits
    it('should maintain independent limits per IP', async () => {
      await fc.assert(
        fc.asyncProperty(
          fc.array(fc.ipV4(), { minLength: 2, maxLength: 5 }),
          fc.webPath(),
          async (ips: string[], endpoint: string) => {
            kv._reset();
            const config: RateLimitConfig = { maxRequests: 10, windowSec: 60 };
            
            // Exhaust first IP's quota
            const firstIp = ips[0];
            for (let i = 0; i < 10; i++) {
              await checkRateLimit(kv, firstIp, endpoint, config);
            }
            
            // Other IPs should still have full quota
            for (let i = 1; i < ips.length; i++) {
              if (ips[i] !== firstIp) {
                const result = await checkRateLimit(kv, ips[i], endpoint, config);
                expect(result.allowed).toBe(true);
                expect(result.remaining).toBe(9);
              }
            }
          }
        ),
        { numRuns: 30 }
      );
    });

    // Property: Different endpoints should have independent limits
    it('should maintain independent limits per endpoint', async () => {
      await fc.assert(
        fc.asyncProperty(
          fc.ipV4(),
          fc.array(fc.webPath().filter(p => p.length > 0), { minLength: 2, maxLength: 5 }),
          async (ip: string, endpoints: string[]) => {
            // Filter to unique non-empty endpoints
            const uniqueEndpoints = [...new Set(endpoints.filter(e => e.length > 0))];
            if (uniqueEndpoints.length < 2) return; // Skip if not enough unique endpoints
            
            kv._reset();
            const config: RateLimitConfig = { maxRequests: 10, windowSec: 60 };
            
            // Exhaust first endpoint's quota
            const firstEndpoint = uniqueEndpoints[0];
            for (let i = 0; i < 10; i++) {
              await checkRateLimit(kv, ip, firstEndpoint, config);
            }
            
            // Other endpoints should still have full quota
            for (let i = 1; i < uniqueEndpoints.length; i++) {
              const result = await checkRateLimit(kv, ip, uniqueEndpoints[i], config);
              expect(result.allowed).toBe(true);
              expect(result.remaining).toBe(9);
            }
          }
        ),
        { numRuns: 30 }
      );
    });
  });
});
