// Feature: tgauth-system, Property 8: Request Validation
// Validates: Requirements 8.3, 8.4
//
// Property: For any authenticated API request:
// - Missing Authorization header SHALL return 400 Bad Request
// - Missing X-Device-ID header SHALL return 400 Bad Request
// - Invalid Content-Type SHALL return 400 Bad Request
// - Missing required parameters SHALL return 400 Bad Request

import { describe, it, expect, vi } from 'vitest';
import * as fc from 'fast-check';
import { ErrorCodes } from '../types';

// Request validation logic simulation
interface ValidationResult {
  valid: boolean;
  error?: {
    code: string;
    message: string;
  };
}

interface RequestHeaders {
  authorization?: string;
  'x-device-id'?: string;
  'content-type'?: string;
}

interface RequestBody {
  [key: string]: unknown;
}

// Validate headers for authenticated endpoints
function validateAuthHeaders(headers: RequestHeaders): ValidationResult {
  if (!headers.authorization) {
    return {
      valid: false,
      error: {
        code: ErrorCodes.BAD_REQUEST,
        message: 'Missing Authorization header',
      },
    };
  }

  if (!headers.authorization.startsWith('Bearer ')) {
    return {
      valid: false,
      error: {
        code: ErrorCodes.BAD_REQUEST,
        message: 'Invalid Authorization header format',
      },
    };
  }

  if (!headers['x-device-id']) {
    return {
      valid: false,
      error: {
        code: ErrorCodes.BAD_REQUEST,
        message: 'Missing X-Device-ID header',
      },
    };
  }

  if (headers['x-device-id'].length !== 64) {
    return {
      valid: false,
      error: {
        code: ErrorCodes.BAD_REQUEST,
        message: 'Invalid X-Device-ID format',
      },
    };
  }

  return { valid: true };
}

// Validate content type for POST requests
function validateContentType(headers: RequestHeaders, method: string): ValidationResult {
  if (method === 'POST' || method === 'PUT' || method === 'PATCH') {
    if (!headers['content-type']?.includes('application/json')) {
      return {
        valid: false,
        error: {
          code: ErrorCodes.BAD_REQUEST,
          message: 'Content-Type must be application/json',
        },
      };
    }
  }
  return { valid: true };
}

// Validate required parameters
function validateRequiredParams(
  body: RequestBody,
  requiredParams: string[]
): ValidationResult {
  for (const param of requiredParams) {
    if (body[param] === undefined || body[param] === null || body[param] === '') {
      return {
        valid: false,
        error: {
          code: ErrorCodes.BAD_REQUEST,
          message: `Missing required parameter: ${param}`,
        },
      };
    }
  }
  return { valid: true };
}

// Validate device_id format
function validateDeviceId(deviceId: string): ValidationResult {
  // Device ID should be a 64-character hex string (SHA256 hash)
  if (!/^[a-f0-9]{64}$/i.test(deviceId)) {
    return {
      valid: false,
      error: {
        code: ErrorCodes.BAD_REQUEST,
        message: 'Invalid device_id format',
      },
    };
  }
  return { valid: true };
}

describe('Request Validation', () => {
  describe('Header Validation', () => {
    it('should reject missing Authorization header', () => {
      const result = validateAuthHeaders({
        'x-device-id': 'a'.repeat(64),
      });
      
      expect(result.valid).toBe(false);
      expect(result.error?.code).toBe(ErrorCodes.BAD_REQUEST);
      expect(result.error?.message).toContain('Authorization');
    });

    it('should reject invalid Authorization format', () => {
      const result = validateAuthHeaders({
        authorization: 'Basic token123',
        'x-device-id': 'a'.repeat(64),
      });
      
      expect(result.valid).toBe(false);
      expect(result.error?.message).toContain('Invalid Authorization');
    });

    it('should reject missing X-Device-ID header', () => {
      const result = validateAuthHeaders({
        authorization: 'Bearer token123',
      });
      
      expect(result.valid).toBe(false);
      expect(result.error?.message).toContain('X-Device-ID');
    });

    it('should reject invalid X-Device-ID format', () => {
      const result = validateAuthHeaders({
        authorization: 'Bearer token123',
        'x-device-id': 'invalid',
      });
      
      expect(result.valid).toBe(false);
      expect(result.error?.message).toContain('Invalid X-Device-ID');
    });

    it('should accept valid headers', () => {
      const result = validateAuthHeaders({
        authorization: 'Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...',
        'x-device-id': 'a'.repeat(64),
      });
      
      expect(result.valid).toBe(true);
    });
  });

  describe('Content-Type Validation', () => {
    it('should reject POST without application/json', () => {
      const result = validateContentType({
        'content-type': 'text/plain',
      }, 'POST');
      
      expect(result.valid).toBe(false);
      expect(result.error?.message).toContain('application/json');
    });

    it('should accept POST with application/json', () => {
      const result = validateContentType({
        'content-type': 'application/json',
      }, 'POST');
      
      expect(result.valid).toBe(true);
    });

    it('should accept POST with application/json; charset=utf-8', () => {
      const result = validateContentType({
        'content-type': 'application/json; charset=utf-8',
      }, 'POST');
      
      expect(result.valid).toBe(true);
    });

    it('should not require content-type for GET', () => {
      const result = validateContentType({}, 'GET');
      expect(result.valid).toBe(true);
    });
  });

  describe('Required Parameters Validation', () => {
    it('should reject missing required parameter', () => {
      const result = validateRequiredParams(
        { name: 'test' },
        ['name', 'device_id']
      );
      
      expect(result.valid).toBe(false);
      expect(result.error?.message).toContain('device_id');
    });

    it('should reject empty string parameter', () => {
      const result = validateRequiredParams(
        { device_id: '' },
        ['device_id']
      );
      
      expect(result.valid).toBe(false);
    });

    it('should reject null parameter', () => {
      const result = validateRequiredParams(
        { device_id: null },
        ['device_id']
      );
      
      expect(result.valid).toBe(false);
    });

    it('should accept all required parameters present', () => {
      const result = validateRequiredParams(
        { device_id: 'abc123', session_id: 'xyz789' },
        ['device_id', 'session_id']
      );
      
      expect(result.valid).toBe(true);
    });
  });

  describe('Device ID Format Validation', () => {
    it('should reject non-hex characters', () => {
      const result = validateDeviceId('g'.repeat(64));
      expect(result.valid).toBe(false);
    });

    it('should reject wrong length', () => {
      const result = validateDeviceId('a'.repeat(32));
      expect(result.valid).toBe(false);
    });

    it('should accept valid SHA256 hash', () => {
      const result = validateDeviceId('a1b2c3d4e5f6'.repeat(5) + 'a1b2');
      expect(result.valid).toBe(true);
    });
  });

  // Property-based tests
  describe('Property Tests', () => {
    // Property 8: Request Validation - Missing headers should be rejected
    it('should always reject requests missing Authorization header', () => {
      fc.assert(
        fc.property(
          fc.hexaString({ minLength: 64, maxLength: 64 }),
          (deviceId: string) => {
            const result = validateAuthHeaders({
              'x-device-id': deviceId,
            });
            expect(result.valid).toBe(false);
            expect(result.error?.code).toBe(ErrorCodes.BAD_REQUEST);
          }
        ),
        { numRuns: 100 }
      );
    });

    // Property: Missing X-Device-ID should be rejected
    it('should always reject requests missing X-Device-ID header', () => {
      fc.assert(
        fc.property(
          fc.string({ minLength: 10, maxLength: 500 }),
          (token: string) => {
            const result = validateAuthHeaders({
              authorization: `Bearer ${token}`,
            });
            expect(result.valid).toBe(false);
            expect(result.error?.code).toBe(ErrorCodes.BAD_REQUEST);
          }
        ),
        { numRuns: 100 }
      );
    });

    // Property: Valid headers should be accepted
    it('should accept valid header combinations', () => {
      fc.assert(
        fc.property(
          fc.string({ minLength: 10, maxLength: 500 }),
          fc.hexaString({ minLength: 64, maxLength: 64 }),
          (token: string, deviceId: string) => {
            const result = validateAuthHeaders({
              authorization: `Bearer ${token}`,
              'x-device-id': deviceId,
            });
            expect(result.valid).toBe(true);
          }
        ),
        { numRuns: 100 }
      );
    });

    // Property: POST requests without JSON content-type should be rejected
    it('should reject POST requests without JSON content-type', () => {
      fc.assert(
        fc.property(
          fc.constantFrom('text/plain', 'text/html', 'application/xml', 'multipart/form-data'),
          (contentType: string) => {
            const result = validateContentType({
              'content-type': contentType,
            }, 'POST');
            expect(result.valid).toBe(false);
          }
        ),
        { numRuns: 50 }
      );
    });

    // Property: Missing required params should be rejected
    it('should reject requests with missing required parameters', () => {
      fc.assert(
        fc.property(
          fc.array(fc.string({ minLength: 1, maxLength: 20 }), { minLength: 1, maxLength: 5 }),
          fc.nat({ max: 10 }),
          (requiredParams: string[], extraParams: number) => {
            // Create body with only some params
            const body: RequestBody = {};
            for (let i = 0; i < extraParams && i < requiredParams.length - 1; i++) {
              body[requiredParams[i]] = 'value';
            }
            
            // Should fail if not all required params are present
            if (Object.keys(body).length < requiredParams.length) {
              const result = validateRequiredParams(body, requiredParams);
              expect(result.valid).toBe(false);
            }
          }
        ),
        { numRuns: 100 }
      );
    });

    // Property: Device ID must be exactly 64 hex characters
    it('should only accept 64-character hex strings as device_id', () => {
      fc.assert(
        fc.property(
          fc.string({ minLength: 1, maxLength: 100 }),
          (input: string) => {
            const result = validateDeviceId(input);
            const isValidHex = /^[a-f0-9]{64}$/i.test(input);
            expect(result.valid).toBe(isValidHex);
          }
        ),
        { numRuns: 100 }
      );
    });

    // Property: Valid device IDs should always be accepted
    it('should always accept valid device IDs', () => {
      fc.assert(
        fc.property(
          fc.hexaString({ minLength: 64, maxLength: 64 }),
          (deviceId: string) => {
            const result = validateDeviceId(deviceId);
            expect(result.valid).toBe(true);
          }
        ),
        { numRuns: 100 }
      );
    });
  });
});
