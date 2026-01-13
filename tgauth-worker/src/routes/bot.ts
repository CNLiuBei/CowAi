import { Hono } from 'hono';
import { Env, ApiResponse, TelegramUpdate, ErrorCodes } from '../types';
import { SessionService } from '../services/session';
import { TelegramService } from '../services/telegram';
import { LicenseService } from '../services/license';

export const botRoutes = new Hono<{ Bindings: Env }>();

// POST /webhook/telegram - Telegram Webhook
botRoutes.post('/telegram', async (c) => {
  // 验证 Telegram 请求 (使用 secret token，如果设置了的话)
  const secretToken = c.req.header('X-Telegram-Bot-Api-Secret-Token');
  // 只有当设置了 secret_token 时才验证
  // 如果没有设置 webhook 的 secret_token，则跳过验证
  if (secretToken && c.env.BOT_SECRET && secretToken !== c.env.BOT_SECRET) {
    return c.json<ApiResponse>({
      success: false,
      error: {
        code: ErrorCodes.UNAUTHORIZED,
        message: 'Invalid webhook secret',
      },
    }, 401);
  }

  const update = await c.req.json<TelegramUpdate>().catch(() => null);
  
  if (!update) {
    return c.json<ApiResponse>({
      success: false,
      error: {
        code: ErrorCodes.BAD_REQUEST,
        message: 'Invalid update format',
      },
    }, 400);
  }

  const telegram = new TelegramService(c.env);
  const sessionService = new SessionService(c.env);

  try {
    // 处理消息
    if (update.message?.text) {
      await handleMessage(c.env, telegram, sessionService, update);
    }
    
    // 处理回调查询 (按钮点击)
    if (update.callback_query) {
      await handleCallbackQuery(c.env, telegram, sessionService, update);
    }

    return c.json({ success: true });
  } catch (error) {
    console.error('Error handling webhook:', error);
    return c.json({ success: true }); // 返回 200 避免 Telegram 重试
  }
});

// 处理消息
async function handleMessage(
  env: Env,
  telegram: TelegramService,
  sessionService: SessionService,
  update: TelegramUpdate
) {
  const message = update.message!;
  const chatId = message.chat.id;
  const text = message.text || '';
  const from = message.from;

  // 处理 /start 命令
  if (text.startsWith('/start')) {
    const parts = text.split(' ');
    
    // 检查是否是登录请求
    if (parts.length > 1 && parts[1].startsWith('LOGIN_')) {
      const sessionId = parts[1].replace('LOGIN_', '');
      await handleLoginRequest(env, telegram, sessionService, chatId, sessionId, from);
      return;
    }

    // 普通 /start 命令
    await telegram.sendMessage(chatId, 
      '👋 欢迎使用 TGAuth 授权系统！\n\n' +
      '此 Bot 用于验证您的软件登录请求。\n' +
      '当您在软件中发起登录时，会收到授权确认消息。\n\n' +
      '📋 *可用命令:*\n' +
      '/help - 显示帮助信息\n' +
      '/status - 查看您的账号状态',
      { parse_mode: 'Markdown' }
    );
    return;
  }

  // 处理 /help 命令
  if (text === '/help') {
    let helpText = '📖 TGAuth 帮助\n\n' +
      '【用户命令】\n' +
      '/start - 开始使用\n' +
      '/status - 查看账号状态\n' +
      '/help - 显示此帮助\n';
    
    if (from && isAdmin(env, from.id.toString())) {
      helpText += '\n【管理员命令】\n' +
        '/genkey <天数> [数量] [设备数] - 生成卡密\n' +
        '  例: /genkey 30 10 1\n' +
        '  例: /genkey permanent 5 2\n' +
        '/keys [状态] - 查看卡密列表\n' +
        '/keyinfo <卡密> - 查看卡密详情\n' +
        '/revoke <卡密> - 撤销卡密\n' +
        '/keystats - 卡密统计\n' +
        '/ban <用户ID> - 封禁用户\n' +
        '/unban <用户ID> - 解封用户\n' +
        '/devices <用户ID> - 查看用户设备\n' +
        '/unbind <用户ID> <设备ID> - 解绑设备\n' +
        '/unbind_all <用户ID> - 解绑所有设备\n' +
        '/stats - 系统统计';
    }
    
    await telegram.sendMessage(chatId, helpText);
    return;
  }

  // 处理 /status 命令
  if (text === '/status' && from) {
    await handleStatusCommand(env, telegram, chatId, from.id.toString());
    return;
  }

  // 处理管理员命令
  if (from && isAdmin(env, from.id.toString())) {
    // 卡密生成命令
    if (text.startsWith('/genkey ')) {
      await handleGenKeyCommand(env, telegram, chatId, text, from.id.toString());
      return;
    }

    // 卡密列表命令
    if (text.startsWith('/keys')) {
      const status = text.replace('/keys', '').trim() || undefined;
      await handleKeysCommand(env, telegram, chatId, status);
      return;
    }

    // 卡密详情命令
    if (text.startsWith('/keyinfo ')) {
      const keyCode = text.replace('/keyinfo ', '').trim();
      await handleKeyInfoCommand(env, telegram, chatId, keyCode);
      return;
    }

    // 撤销卡密命令
    if (text.startsWith('/revoke ')) {
      const keyCode = text.replace('/revoke ', '').trim();
      await handleRevokeCommand(env, telegram, chatId, keyCode);
      return;
    }

    // 卡密统计命令
    if (text === '/keystats') {
      await handleKeyStatsCommand(env, telegram, chatId);
      return;
    }

    if (text.startsWith('/ban ')) {
      const tgUserId = text.replace('/ban ', '').trim();
      await handleBanCommand(env, telegram, chatId, tgUserId, true);
      return;
    }

    if (text.startsWith('/unban ')) {
      const tgUserId = text.replace('/unban ', '').trim();
      await handleBanCommand(env, telegram, chatId, tgUserId, false);
      return;
    }

    if (text.startsWith('/devices ')) {
      const tgUserId = text.replace('/devices ', '').trim();
      await handleDevicesCommand(env, telegram, chatId, tgUserId);
      return;
    }

    if (text.startsWith('/unbind ')) {
      const args = text.replace('/unbind ', '').trim().split(' ');
      if (args.length === 2) {
        await handleUnbindCommand(env, telegram, chatId, args[0], args[1]);
      } else {
        await telegram.sendMessage(chatId, '❌ 用法: /unbind <tg_user_id> <device_id>');
      }
      return;
    }

    if (text.startsWith('/unbind_all ')) {
      const tgUserId = text.replace('/unbind_all ', '').trim();
      await handleUnbindAllCommand(env, telegram, chatId, tgUserId);
      return;
    }

    if (text === '/stats') {
      await handleStatsCommand(env, telegram, chatId);
      return;
    }
  }
}

// 处理登录请求
async function handleLoginRequest(
  env: Env,
  telegram: TelegramService,
  sessionService: SessionService,
  chatId: number,
  sessionId: string,
  from?: { id: number; first_name: string; username?: string }
) {
  // 获取会话
  const session = await sessionService.getSession(sessionId);
  
  if (!session) {
    await telegram.sendMessage(chatId, '❌ 登录请求无效或已过期，请重新发起登录。');
    return;
  }

  if (session.expires_at < Date.now() / 1000) {
    await telegram.sendMessage(chatId, '❌ 登录请求已过期，请重新发起登录。');
    return;
  }

  if (session.status !== 'pending') {
    await telegram.sendMessage(chatId, '❌ 此登录请求已被处理。');
    return;
  }

  // 检查用户是否被封禁
  if (from) {
    const user = await env.DB.prepare(
      'SELECT is_banned FROM users WHERE tg_user_id = ?'
    ).bind(from.id.toString()).first<{ is_banned: number }>();

    if (user?.is_banned) {
      await telegram.sendMessage(chatId, '❌ 您的账号已被封禁，无法登录。');
      return;
    }
  }

  // 发送授权确认消息
  await telegram.sendMessage(
    chatId,
    '🔐 *登录授权请求*\n\n' +
    '有新设备请求登录您的账号。\n\n' +
    `设备 ID: \`${session.device_id.substring(0, 16)}...\`\n` +
    `请求时间: ${new Date(session.created_at * 1000).toLocaleString()}\n\n` +
    '请确认是否允许此设备登录？',
    {
      parse_mode: 'Markdown',
      reply_markup: {
        inline_keyboard: [
          [
            { text: '✅ 同意', callback_data: `approve:${sessionId}` },
            { text: '❌ 拒绝', callback_data: `reject:${sessionId}` },
          ],
        ],
      },
    }
  );
}

// 处理回调查询
async function handleCallbackQuery(
  env: Env,
  telegram: TelegramService,
  sessionService: SessionService,
  update: TelegramUpdate
) {
  const query = update.callback_query!;
  const data = query.data || '';
  const from = query.from;
  const messageId = query.message?.message_id;
  const chatId = query.message?.chat.id;

  if (!chatId || !messageId) {
    await telegram.answerCallbackQuery(query.id, '❌ 操作失败');
    return;
  }

  // 处理授权/拒绝
  if (data.startsWith('approve:') || data.startsWith('reject:')) {
    const [action, sessionId] = data.split(':');
    const session = await sessionService.getSession(sessionId);

    if (!session) {
      await telegram.answerCallbackQuery(query.id, '❌ 会话已过期');
      await telegram.editMessage(chatId, messageId, '❌ 登录请求已过期或无效。');
      return;
    }

    if (session.status !== 'pending') {
      await telegram.answerCallbackQuery(query.id, '❌ 已处理');
      return;
    }

    if (action === 'approve') {
      // 批准登录
      await sessionService.approveSession(sessionId, from.id.toString());
      
      // 创建或更新用户
      await env.DB.prepare(`
        INSERT INTO users (tg_user_id, username, first_name, is_banned, device_limit, created_at, updated_at)
        VALUES (?, ?, ?, 0, ?, ?, ?)
        ON CONFLICT(tg_user_id) DO UPDATE SET
          username = excluded.username,
          first_name = excluded.first_name,
          updated_at = excluded.updated_at
      `).bind(
        from.id.toString(),
        from.username || null,
        from.first_name || null,
        parseInt(env.DEFAULT_DEVICE_LIMIT),
        Math.floor(Date.now() / 1000),
        Math.floor(Date.now() / 1000)
      ).run();

      // 绑定设备
      await env.DB.prepare(`
        INSERT INTO devices (device_id, tg_user_id, bound_at, last_seen, is_active)
        VALUES (?, ?, ?, ?, 1)
        ON CONFLICT(device_id, tg_user_id) DO UPDATE SET
          last_seen = excluded.last_seen,
          is_active = 1
      `).bind(
        session.device_id,
        from.id.toString(),
        Math.floor(Date.now() / 1000),
        Math.floor(Date.now() / 1000)
      ).run();

      await telegram.answerCallbackQuery(query.id, '✅ 已授权');
      await telegram.editMessage(chatId, messageId, 
        '✅ *登录已授权*\n\n' +
        '设备已成功登录您的账号。\n' +
        '如果这不是您的操作，请立即使用 /unbind_all 命令解绑所有设备。',
        { parse_mode: 'Markdown' }
      );
    } else {
      // 拒绝登录
      await sessionService.rejectSession(sessionId);
      
      await telegram.answerCallbackQuery(query.id, '❌ 已拒绝');
      await telegram.editMessage(chatId, messageId, 
        '❌ *登录已拒绝*\n\n' +
        '此设备的登录请求已被拒绝。',
        { parse_mode: 'Markdown' }
      );
    }
  }
}

// 检查是否是管理员
function isAdmin(env: Env, tgUserId: string): boolean {
  const adminIds = (env.ADMIN_TG_IDS || '').split(',').map(id => id.trim());
  return adminIds.includes(tgUserId);
}

// 处理封禁命令
async function handleBanCommand(
  env: Env,
  telegram: TelegramService,
  chatId: number,
  tgUserId: string,
  ban: boolean
) {
  try {
    await env.DB.prepare(
      'UPDATE users SET is_banned = ?, updated_at = ? WHERE tg_user_id = ?'
    ).bind(ban ? 1 : 0, Math.floor(Date.now() / 1000), tgUserId).run();

    if (ban) {
      await env.KV.put(`banned:${tgUserId}`, '1', { expirationTtl: 60 * 60 * 24 * 365 });
      await telegram.sendMessage(chatId, `✅ 用户 ${tgUserId} 已被封禁`);
    } else {
      await env.KV.delete(`banned:${tgUserId}`);
      await telegram.sendMessage(chatId, `✅ 用户 ${tgUserId} 已被解封`);
    }
  } catch (error) {
    await telegram.sendMessage(chatId, `❌ 操作失败: ${error}`);
  }
}

// 处理设备查询命令
async function handleDevicesCommand(
  env: Env,
  telegram: TelegramService,
  chatId: number,
  tgUserId: string
) {
  try {
    const devices = await env.DB.prepare(
      'SELECT device_id, bound_at, last_seen, is_active FROM devices WHERE tg_user_id = ?'
    ).bind(tgUserId).all<{ device_id: string; bound_at: number; last_seen: number; is_active: number }>();

    if (!devices.results || devices.results.length === 0) {
      await telegram.sendMessage(chatId, `用户 ${tgUserId} 没有绑定的设备`);
      return;
    }

    let message = `📱 *用户 ${tgUserId} 的设备列表*\n\n`;
    for (const device of devices.results) {
      const status = device.is_active ? '✅ 活跃' : '❌ 已解绑';
      message += `设备: \`${device.device_id.substring(0, 16)}...\`\n`;
      message += `状态: ${status}\n`;
      message += `绑定时间: ${new Date(device.bound_at * 1000).toLocaleString()}\n`;
      message += `最后活跃: ${new Date(device.last_seen * 1000).toLocaleString()}\n\n`;
    }

    await telegram.sendMessage(chatId, message, { parse_mode: 'Markdown' });
  } catch (error) {
    await telegram.sendMessage(chatId, `❌ 查询失败: ${error}`);
  }
}

// 处理解绑命令
async function handleUnbindCommand(
  env: Env,
  telegram: TelegramService,
  chatId: number,
  tgUserId: string,
  deviceId: string
) {
  try {
    await env.DB.prepare(
      'UPDATE devices SET is_active = 0 WHERE tg_user_id = ? AND device_id = ?'
    ).bind(tgUserId, deviceId).run();

    await env.KV.put(`unbind:${deviceId}`, '1', { expirationTtl: 60 * 60 * 24 * 7 });

    await telegram.sendMessage(chatId, `✅ 设备 ${deviceId.substring(0, 16)}... 已从用户 ${tgUserId} 解绑`);
  } catch (error) {
    await telegram.sendMessage(chatId, `❌ 解绑失败: ${error}`);
  }
}

// 处理解绑所有设备命令
async function handleUnbindAllCommand(
  env: Env,
  telegram: TelegramService,
  chatId: number,
  tgUserId: string
) {
  try {
    const devices = await env.DB.prepare(
      'SELECT device_id FROM devices WHERE tg_user_id = ? AND is_active = 1'
    ).bind(tgUserId).all<{ device_id: string }>();

    await env.DB.prepare(
      'UPDATE devices SET is_active = 0 WHERE tg_user_id = ?'
    ).bind(tgUserId).run();

    if (devices.results) {
      for (const device of devices.results) {
        await env.KV.put(`unbind:${device.device_id}`, '1', { expirationTtl: 60 * 60 * 24 * 7 });
      }
    }

    await telegram.sendMessage(chatId, `✅ 用户 ${tgUserId} 的所有设备 (${devices.results?.length || 0}) 已解绑`);
  } catch (error) {
    await telegram.sendMessage(chatId, `❌ 解绑失败: ${error}`);
  }
}

// 处理统计命令
async function handleStatsCommand(
  env: Env,
  telegram: TelegramService,
  chatId: number
) {
  try {
    const licenseService = new LicenseService(env);
    
    const [users, devices, sessions, banned, keyStats] = await Promise.all([
      env.DB.prepare('SELECT COUNT(*) as count FROM users').first<{ count: number }>(),
      env.DB.prepare('SELECT COUNT(*) as count FROM devices WHERE is_active = 1').first<{ count: number }>(),
      env.DB.prepare('SELECT COUNT(*) as count FROM sessions WHERE status = ?').bind('pending').first<{ count: number }>(),
      env.DB.prepare('SELECT COUNT(*) as count FROM users WHERE is_banned = 1').first<{ count: number }>(),
      licenseService.getStats(),
    ]);

    const message = 
      '📊 *系统统计*\n\n' +
      '*用户数据:*\n' +
      `👥 总用户数: ${users?.count || 0}\n` +
      `📱 活跃设备: ${devices?.count || 0}\n` +
      `⏳ 待处理会话: ${sessions?.count || 0}\n` +
      `🚫 封禁用户: ${banned?.count || 0}\n\n` +
      '*卡密数据:*\n' +
      `🔑 总卡密: ${keyStats.total}\n` +
      `⬜ 未使用: ${keyStats.unused}\n` +
      `✅ 已激活: ${keyStats.activated}\n` +
      `⏰ 已过期: ${keyStats.expired}\n` +
      `❌ 已撤销: ${keyStats.revoked}\n` +
      `\n⏰ 更新时间: ${new Date().toLocaleString()}`;

    await telegram.sendMessage(chatId, message, { parse_mode: 'Markdown' });
  } catch (error) {
    await telegram.sendMessage(chatId, `❌ 获取统计失败: ${error}`);
  }
}

// 处理用户状态命令
async function handleStatusCommand(
  env: Env,
  telegram: TelegramService,
  chatId: number,
  tgUserId: string
) {
  try {
    const user = await env.DB.prepare(
      'SELECT * FROM users WHERE tg_user_id = ?'
    ).bind(tgUserId).first<{ tg_user_id: string; username: string; is_banned: number; device_limit: number; created_at: number }>();

    if (!user) {
      await telegram.sendMessage(chatId, '您还没有登录记录。请在软件中发起登录。');
      return;
    }

    const devices = await env.DB.prepare(
      'SELECT COUNT(*) as count FROM devices WHERE tg_user_id = ? AND is_active = 1'
    ).bind(tgUserId).first<{ count: number }>();

    const status = user.is_banned ? '🚫 已封禁' : '✅ 正常';
    const message = 
      '👤 *您的账号状态*\n\n' +
      `状态: ${status}\n` +
      `设备数: ${devices?.count || 0}/${user.device_limit}\n` +
      `注册时间: ${new Date(user.created_at * 1000).toLocaleString()}`;

    await telegram.sendMessage(chatId, message, { parse_mode: 'Markdown' });
  } catch (error) {
    await telegram.sendMessage(chatId, `❌ 查询失败: ${error}`);
  }
}

// 处理生成卡密命令
async function handleGenKeyCommand(
  env: Env,
  telegram: TelegramService,
  chatId: number,
  text: string,
  createdBy: string
) {
  try {
    // 解析命令: /genkey <天数|permanent> [数量] [设备数]
    const parts = text.replace('/genkey ', '').trim().split(/\s+/);
    
    if (parts.length < 1) {
      await telegram.sendMessage(chatId, 
        '❌ 用法: /genkey <天数|permanent> [数量] [设备数]\n' +
        '例: /genkey 30 10 1\n' +
        '例: /genkey permanent 5 2'
      );
      return;
    }

    const isPermanent = parts[0].toLowerCase() === 'permanent';
    const durationDays = isPermanent ? undefined : parseInt(parts[0]);
    const count = parseInt(parts[1] || '1');
    const maxDevices = parseInt(parts[2] || '1');

    if (!isPermanent && (isNaN(durationDays!) || durationDays! <= 0)) {
      await telegram.sendMessage(chatId, '❌ 天数必须是正整数，或使用 "permanent" 表示永久');
      return;
    }

    if (isNaN(count) || count <= 0 || count > 100) {
      await telegram.sendMessage(chatId, '❌ 数量必须是 1-100 之间的整数');
      return;
    }

    if (isNaN(maxDevices) || maxDevices <= 0 || maxDevices > 10) {
      await telegram.sendMessage(chatId, '❌ 设备数必须是 1-10 之间的整数');
      return;
    }

    const licenseService = new LicenseService(env);
    const keys = await licenseService.generateKeys({
      count,
      keyType: isPermanent ? 'permanent' : 'time',
      durationDays,
      maxDevices,
      createdBy,
    });

    let message = `✅ *成功生成 ${keys.length} 个卡密*\n\n`;
    message += `类型: ${isPermanent ? '永久' : `${durationDays}天`}\n`;
    message += `设备数: ${maxDevices}\n\n`;
    message += '```\n';
    message += keys.join('\n');
    message += '\n```';

    await telegram.sendMessage(chatId, message, { parse_mode: 'Markdown' });
  } catch (error) {
    await telegram.sendMessage(chatId, `❌ 生成失败: ${error}`);
  }
}

// 处理卡密列表命令
async function handleKeysCommand(
  env: Env,
  telegram: TelegramService,
  chatId: number,
  status?: string
) {
  try {
    const licenseService = new LicenseService(env);
    const result = await licenseService.listKeys({ status, limit: 20 });

    if (result.keys.length === 0) {
      await telegram.sendMessage(chatId, status ? `没有 ${status} 状态的卡密` : '没有卡密');
      return;
    }

    let message = `🔑 *卡密列表* (${result.keys.length}/${result.total})\n\n`;
    
    for (const key of result.keys) {
      const statusEmoji = {
        'unused': '⬜',
        'activated': '✅',
        'expired': '⏰',
        'revoked': '❌'
      }[key.status] || '❓';
      
      const typeText = key.key_type === 'permanent' ? '永久' : `${key.duration_days}天`;
      message += `${statusEmoji} \`${key.key_code}\`\n`;
      message += `   ${typeText} | ${key.max_devices}设备\n`;
    }

    if (result.total > 20) {
      message += `\n... 还有 ${result.total - 20} 个卡密`;
    }

    await telegram.sendMessage(chatId, message, { parse_mode: 'Markdown' });
  } catch (error) {
    await telegram.sendMessage(chatId, `❌ 查询失败: ${error}`);
  }
}

// 处理卡密详情命令
async function handleKeyInfoCommand(
  env: Env,
  telegram: TelegramService,
  chatId: number,
  keyCode: string
) {
  try {
    const licenseService = new LicenseService(env);
    const key = await licenseService.getKey(keyCode);

    if (!key) {
      await telegram.sendMessage(chatId, '❌ 卡密不存在');
      return;
    }

    const devices = await licenseService.getKeyDevices(key.id);
    const statusEmoji = {
      'unused': '⬜ 未使用',
      'activated': '✅ 已激活',
      'expired': '⏰ 已过期',
      'revoked': '❌ 已撤销'
    }[key.status] || '❓ 未知';

    let message = `🔑 *卡密详情*\n\n`;
    message += `卡密: \`${key.key_code}\`\n`;
    message += `状态: ${statusEmoji}\n`;
    message += `类型: ${key.key_type === 'permanent' ? '永久' : `${key.duration_days}天`}\n`;
    message += `设备限制: ${key.max_devices}\n`;
    message += `创建时间: ${new Date(key.created_at * 1000).toLocaleString()}\n`;
    
    if (key.activated_at) {
      message += `激活时间: ${new Date(key.activated_at * 1000).toLocaleString()}\n`;
    }
    
    if (key.expires_at) {
      const now = Math.floor(Date.now() / 1000);
      const remaining = key.expires_at - now;
      const remainingDays = Math.ceil(remaining / 86400);
      message += `过期时间: ${new Date(key.expires_at * 1000).toLocaleString()}`;
      if (remaining > 0) {
        message += ` (剩余 ${remainingDays} 天)`;
      }
      message += '\n';
    }

    if (devices.length > 0) {
      message += `\n📱 *绑定设备 (${devices.length}):*\n`;
      for (const device of devices) {
        const status = device.is_active ? '✅' : '❌';
        message += `${status} \`${device.device_id.substring(0, 16)}...\`\n`;
        message += `   最后活跃: ${new Date(device.last_seen * 1000).toLocaleString()}\n`;
      }
    }

    await telegram.sendMessage(chatId, message, { parse_mode: 'Markdown' });
  } catch (error) {
    await telegram.sendMessage(chatId, `❌ 查询失败: ${error}`);
  }
}

// 处理撤销卡密命令
async function handleRevokeCommand(
  env: Env,
  telegram: TelegramService,
  chatId: number,
  keyCode: string
) {
  try {
    const licenseService = new LicenseService(env);
    const success = await licenseService.revokeKey(keyCode);

    if (success) {
      await telegram.sendMessage(chatId, `✅ 卡密 \`${keyCode}\` 已撤销`, { parse_mode: 'Markdown' });
    } else {
      await telegram.sendMessage(chatId, '❌ 卡密不存在');
    }
  } catch (error) {
    await telegram.sendMessage(chatId, `❌ 撤销失败: ${error}`);
  }
}

// 处理卡密统计命令
async function handleKeyStatsCommand(
  env: Env,
  telegram: TelegramService,
  chatId: number
) {
  try {
    const licenseService = new LicenseService(env);
    const stats = await licenseService.getStats();

    const message = 
      '🔑 *卡密统计*\n\n' +
      `总数: ${stats.total}\n` +
      `⬜ 未使用: ${stats.unused}\n` +
      `✅ 已激活: ${stats.activated}\n` +
      `⏰ 已过期: ${stats.expired}\n` +
      `❌ 已撤销: ${stats.revoked}`;

    await telegram.sendMessage(chatId, message, { parse_mode: 'Markdown' });
  } catch (error) {
    await telegram.sendMessage(chatId, `❌ 获取统计失败: ${error}`);
  }
}
