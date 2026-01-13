import { Hono } from 'hono';
import { cors } from 'hono/cors';
import { Env, ApiResponse, ErrorCodes } from './types';
import { authRoutes } from './routes/auth';
import { adminRoutes } from './routes/admin';
import { botRoutes } from './routes/bot';
import { licenseRoutes } from './routes/license';
import { payRoutes } from './routes/pay';
import { rateLimitMiddleware } from './middleware/rateLimit';
import { errorHandler } from './middleware/errorHandler';
import { cleanupExpiredData } from './services/cleanup';

// 创建 Hono 应用
const app = new Hono<{ Bindings: Env }>();

// 全局中间件
app.use('*', cors({
  origin: '*',
  allowMethods: ['GET', 'POST', 'PUT', 'DELETE', 'OPTIONS'],
  allowHeaders: ['Content-Type', 'Authorization', 'X-Device-ID', 'X-Bot-Secret', 'X-Admin-Secret', 'X-Admin-ID'],
}));

// 错误处理中间件
app.use('*', errorHandler);

// Rate Limiting 中间件 (除了 webhook)
app.use('/api/*', rateLimitMiddleware);

// 健康检查
app.get('/health', (c) => {
  return c.json<ApiResponse>({
    success: true,
    data: {
      status: 'healthy',
      timestamp: Date.now(),
    },
  });
});

// 设置 Bot 命令菜单 (管理员用)
app.get('/setup-commands', async (c) => {
  const commands = [
    { command: 'start', description: '开始使用' },
    { command: 'help', description: '显示帮助' },
    { command: 'status', description: '查看账号状态' },
    { command: 'genkey', description: '生成卡密 (管理员)' },
    { command: 'keys', description: '查看卡密列表 (管理员)' },
    { command: 'keyinfo', description: '查看卡密详情 (管理员)' },
    { command: 'revoke', description: '撤销卡密 (管理员)' },
    { command: 'keystats', description: '卡密统计 (管理员)' },
    { command: 'ban', description: '封禁用户 (管理员)' },
    { command: 'unban', description: '解封用户 (管理员)' },
    { command: 'devices', description: '查看用户设备 (管理员)' },
    { command: 'stats', description: '系统统计 (管理员)' },
  ];

  try {
    const response = await fetch(
      `https://api.telegram.org/bot${c.env.BOT_TOKEN}/setMyCommands`,
      {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ commands }),
      }
    );
    const result = await response.json();
    return c.json({ success: true, result });
  } catch (error) {
    return c.json({ success: false, error: String(error) }, 500);
  }
});

// API 路由
app.route('/api', authRoutes);
app.route('/api', licenseRoutes);  // 卡密路由
app.route('/api/pay', payRoutes);  // 支付路由
app.route('/api/admin', adminRoutes);

// Telegram Webhook 路由
app.route('/webhook', botRoutes);

// 404 处理 - 只对 API 路由返回 JSON
app.notFound((c) => {
  const path = c.req.path;
  // 只有 /api 和 /webhook 路径返回 JSON 404
  if (path.startsWith('/api') || path.startsWith('/webhook') || path === '/health' || path === '/setup-commands') {
    return c.json<ApiResponse>({
      success: false,
      error: {
        code: 'NOT_FOUND',
        message: 'Endpoint not found',
      },
    }, 404);
  }
  // 其他路径交给静态资源处理（返回 index.html 用于 SPA）
  return c.env.ASSETS.fetch(new Request(new URL('/index.html', c.req.url)));
});

// 全局错误处理
app.onError((err, c) => {
  console.error('Unhandled error:', err);
  return c.json<ApiResponse>({
    success: false,
    error: {
      code: ErrorCodes.INTERNAL_ERROR,
      message: 'An unexpected error occurred',
    },
  }, 500);
});

// 导出 Worker
export default {
  fetch: app.fetch,
  
  // 定时任务处理器 - 每天凌晨 3 点执行
  async scheduled(event: ScheduledEvent, env: Env, ctx: ExecutionContext) {
    ctx.waitUntil(cleanupExpiredData(env));
  },
};
