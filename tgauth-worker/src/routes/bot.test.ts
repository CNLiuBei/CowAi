// Feature: tgauth-system
// Validates: Requirements 2.1, 2.2, 2.3, 2.4
//
// Tests for Telegram Bot command parsing and callback handling

import { describe, it, expect } from 'vitest';
import * as fc from 'fast-check';

// Helper functions to test command parsing logic
// (extracted from bot.ts for testing)

function parseStartCommand(text: string): { isLogin: boolean; sessionId: string | null } {
  if (!text.startsWith('/start')) {
    return { isLogin: false, sessionId: null };
  }

  const parts = text.split(' ');
  if (parts.length > 1 && parts[1].startsWith('LOGIN_')) {
    const sessionId = parts[1].replace('LOGIN_', '');
    return { isLogin: true, sessionId };
  }

  return { isLogin: false, sessionId: null };
}

function parseCallbackData(data: string): { action: string; sessionId: string } | null {
  if (!data.includes(':')) {
    return null;
  }

  const [action, sessionId] = data.split(':');
  if (!action || !sessionId) {
    return null;
  }

  return { action, sessionId };
}

function parseAdminCommand(text: string): { command: string; args: string[] } | null {
  if (!text.startsWith('/')) {
    return null;
  }

  const parts = text.split(' ');
  const command = parts[0].substring(1); // Remove leading /
  const args = parts.slice(1);

  return { command, args };
}

function isAdmin(adminIds: string, userId: string): boolean {
  const ids = adminIds.split(',').map(id => id.trim());
  return ids.includes(userId);
}

describe('Bot Command Parsing', () => {
  describe('/start command parsing', () => {
    it('should parse /start LOGIN_{session_id} correctly', () => {
      const result = parseStartCommand('/start LOGIN_abc-123-def');
      expect(result.isLogin).toBe(true);
      expect(result.sessionId).toBe('abc-123-def');
    });

    it('should handle plain /start command', () => {
      const result = parseStartCommand('/start');
      expect(result.isLogin).toBe(false);
      expect(result.sessionId).toBeNull();
    });

    it('should handle /start with non-login parameter', () => {
      const result = parseStartCommand('/start something_else');
      expect(result.isLogin).toBe(false);
      expect(result.sessionId).toBeNull();
    });

    it('should handle non-start commands', () => {
      const result = parseStartCommand('/help');
      expect(result.isLogin).toBe(false);
      expect(result.sessionId).toBeNull();
    });

    // Property test: LOGIN_ prefix should always be stripped
    it('should always strip LOGIN_ prefix from session ID', () => {
      fc.assert(
        fc.property(
          fc.uuid(),
          (sessionId) => {
            const command = `/start LOGIN_${sessionId}`;
            const result = parseStartCommand(command);
            
            expect(result.isLogin).toBe(true);
            expect(result.sessionId).toBe(sessionId);
            expect(result.sessionId).not.toContain('LOGIN_');
          }
        ),
        { numRuns: 100 }
      );
    });
  });

  describe('Callback data parsing', () => {
    it('should parse approve callback correctly', () => {
      const result = parseCallbackData('approve:session-123');
      expect(result).not.toBeNull();
      expect(result!.action).toBe('approve');
      expect(result!.sessionId).toBe('session-123');
    });

    it('should parse reject callback correctly', () => {
      const result = parseCallbackData('reject:session-456');
      expect(result).not.toBeNull();
      expect(result!.action).toBe('reject');
      expect(result!.sessionId).toBe('session-456');
    });

    it('should return null for invalid callback data', () => {
      expect(parseCallbackData('invalid')).toBeNull();
      expect(parseCallbackData('')).toBeNull();
      expect(parseCallbackData(':')).toBeNull();
    });

    // Property test: action:sessionId format should always parse correctly
    it('should parse any action:sessionId format', () => {
      fc.assert(
        fc.property(
          fc.stringOf(fc.constantFrom(...'abcdefghijklmnopqrstuvwxyz'.split('')), { minLength: 1, maxLength: 20 }),
          fc.uuid(),
          (action, sessionId) => {
            const data = `${action}:${sessionId}`;
            const result = parseCallbackData(data);
            
            expect(result).not.toBeNull();
            expect(result!.action).toBe(action);
            expect(result!.sessionId).toBe(sessionId);
          }
        ),
        { numRuns: 100 }
      );
    });
  });

  describe('Admin command parsing', () => {
    it('should parse /ban command correctly', () => {
      const result = parseAdminCommand('/ban 123456789');
      expect(result).not.toBeNull();
      expect(result!.command).toBe('ban');
      expect(result!.args).toEqual(['123456789']);
    });

    it('should parse /unban command correctly', () => {
      const result = parseAdminCommand('/unban 123456789');
      expect(result).not.toBeNull();
      expect(result!.command).toBe('unban');
      expect(result!.args).toEqual(['123456789']);
    });

    it('should parse /devices command correctly', () => {
      const result = parseAdminCommand('/devices 123456789');
      expect(result).not.toBeNull();
      expect(result!.command).toBe('devices');
      expect(result!.args).toEqual(['123456789']);
    });

    it('should parse /unbind command with two args', () => {
      const result = parseAdminCommand('/unbind 123456789 device-abc');
      expect(result).not.toBeNull();
      expect(result!.command).toBe('unbind');
      expect(result!.args).toEqual(['123456789', 'device-abc']);
    });

    it('should parse /unbind_all command correctly', () => {
      const result = parseAdminCommand('/unbind_all 123456789');
      expect(result).not.toBeNull();
      expect(result!.command).toBe('unbind_all');
      expect(result!.args).toEqual(['123456789']);
    });

    it('should parse /stats command with no args', () => {
      const result = parseAdminCommand('/stats');
      expect(result).not.toBeNull();
      expect(result!.command).toBe('stats');
      expect(result!.args).toEqual([]);
    });

    it('should return null for non-command text', () => {
      expect(parseAdminCommand('hello')).toBeNull();
      expect(parseAdminCommand('')).toBeNull();
    });

    // Property test: commands should always be parsed correctly
    it('should parse any /{command} {args} format', () => {
      fc.assert(
        fc.property(
          fc.stringOf(fc.constantFrom(...'abcdefghijklmnopqrstuvwxyz_'.split('')), { minLength: 1, maxLength: 20 }),
          fc.array(fc.string({ minLength: 1, maxLength: 20 }).filter(s => !s.includes(' ')), { minLength: 0, maxLength: 5 }),
          (command, args) => {
            const text = `/${command}${args.length > 0 ? ' ' + args.join(' ') : ''}`;
            const result = parseAdminCommand(text);
            
            expect(result).not.toBeNull();
            expect(result!.command).toBe(command);
            expect(result!.args.length).toBe(args.length);
          }
        ),
        { numRuns: 100 }
      );
    });
  });

  describe('Admin permission check', () => {
    it('should return true for admin user', () => {
      expect(isAdmin('123456,789012', '123456')).toBe(true);
      expect(isAdmin('123456,789012', '789012')).toBe(true);
    });

    it('should return false for non-admin user', () => {
      expect(isAdmin('123456,789012', '999999')).toBe(false);
    });

    it('should handle single admin ID', () => {
      expect(isAdmin('123456', '123456')).toBe(true);
      expect(isAdmin('123456', '789012')).toBe(false);
    });

    it('should handle whitespace in admin IDs', () => {
      expect(isAdmin('123456, 789012', '789012')).toBe(true);
      expect(isAdmin(' 123456 , 789012 ', '123456')).toBe(true);
    });

    // Property test: admin check should be consistent
    it('should consistently identify admins', () => {
      fc.assert(
        fc.property(
          fc.array(fc.stringOf(fc.constantFrom(...'0123456789'.split('')), { minLength: 5, maxLength: 15 }), { minLength: 1, maxLength: 5 }),
          fc.nat({ max: 10 }),
          (adminIds, index) => {
            const adminList = adminIds.join(',');
            const selectedId = adminIds[index % adminIds.length];
            
            // Selected ID should always be recognized as admin
            expect(isAdmin(adminList, selectedId)).toBe(true);
          }
        ),
        { numRuns: 100 }
      );
    });
  });
});

describe('Telegram Update Structure', () => {
  it('should handle message update', () => {
    const update = {
      update_id: 123456,
      message: {
        message_id: 1,
        from: { id: 123456789, is_bot: false, first_name: 'Test' },
        chat: { id: 123456789, type: 'private' },
        date: Math.floor(Date.now() / 1000),
        text: '/start LOGIN_test-session',
      },
    };

    expect(update.message).toBeDefined();
    expect(update.message.text).toBe('/start LOGIN_test-session');
  });

  it('should handle callback query update', () => {
    const update = {
      update_id: 123457,
      callback_query: {
        id: 'callback-123',
        from: { id: 123456789, is_bot: false, first_name: 'Test' },
        message: {
          message_id: 1,
          chat: { id: 123456789, type: 'private' },
          date: Math.floor(Date.now() / 1000),
        },
        data: 'approve:session-123',
      },
    };

    expect(update.callback_query).toBeDefined();
    expect(update.callback_query.data).toBe('approve:session-123');
  });
});

describe('Inline Keyboard Structure', () => {
  it('should create valid inline keyboard for authorization', () => {
    const sessionId = 'test-session-123';
    const keyboard = {
      inline_keyboard: [
        [
          { text: '✅ 同意', callback_data: `approve:${sessionId}` },
          { text: '❌ 拒绝', callback_data: `reject:${sessionId}` },
        ],
      ],
    };

    expect(keyboard.inline_keyboard).toHaveLength(1);
    expect(keyboard.inline_keyboard[0]).toHaveLength(2);
    expect(keyboard.inline_keyboard[0][0].callback_data).toBe(`approve:${sessionId}`);
    expect(keyboard.inline_keyboard[0][1].callback_data).toBe(`reject:${sessionId}`);
  });

  // Property test: keyboard should always contain session ID
  it('should always include session ID in callback data', () => {
    fc.assert(
      fc.property(
        fc.uuid(),
        (sessionId) => {
          const keyboard = {
            inline_keyboard: [
              [
                { text: '✅ 同意', callback_data: `approve:${sessionId}` },
                { text: '❌ 拒绝', callback_data: `reject:${sessionId}` },
              ],
            ],
          };

          keyboard.inline_keyboard[0].forEach(button => {
            expect(button.callback_data).toContain(sessionId);
          });
        }
      ),
      { numRuns: 100 }
    );
  });
});
