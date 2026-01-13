import { Context, Next } from 'hono';
import { Env, ApiResponse, ErrorCodes } from '../types';

// Rate Limiting 中间件
export async function rateLimitMiddleware(c: Context<{ Bindings: Env }>, next: Next) {
  const ip = c.req.header('CF-Connecting-IP') || 'unknown';
  const path = new URL(c.req.url).pathname;
  
  // 构建限流 key
  const key = `ratelimit:${ip}:${path}`;
  
  const maxRequests = parseInt(c.env.RATE_LIMIT_REQUESTS) || 60;
  const windowSec = parseInt(c.env.RATE_LIMIT_WINDOW_SEC) || 60;

  try {
    // 获取当前计数
    const currentCount = await c.env.KV.get(key);
    const count = currentCount ? parseInt(currentCount) : 0;

    if (count >= maxRequests) {
      return c.json<ApiResponse>({
        success: false,
        error: {
          code: ErrorCodes.RATE_LIMITED,
          message: 'Too many requests, please try again later',
        },
      }, 429, {
        'Retry-After': windowSec.toString(),
        'X-RateLimit-Limit': maxRequests.toString(),
        'X-RateLimit-Remaining': '0',
        'X-RateLimit-Reset': (Math.floor(Date.now() / 1000) + windowSec).toString(),
      });
    }

    // 增加计数
    await c.env.KV.put(key, (count + 1).toString(), {
      expirationTtl: windowSec,
    });

    // 添加限流响应头
    c.header('X-RateLimit-Limit', maxRequests.toString());
    c.header('X-RateLimit-Remaining', (maxRequests - count - 1).toString());

    await next();
  } catch (error) {
    // 限流失败不应阻止请求
    console.error('Rate limit error:', error);
    await next();
  }
}

// 更严格的限流 (用于敏感操作)
export async function strictRateLimitMiddleware(c: Context<{ Bindings: Env }>, next: Next) {
  const ip = c.req.header('CF-Connecting-IP') || 'unknown';
  const path = new URL(c.req.url).pathname;
  
  const key = `ratelimit:strict:${ip}:${path}`;
  
  // 更严格的限制: 每分钟 10 次
  const maxRequests = 10;
  const windowSec = 60;

  try {
    const currentCount = await c.env.KV.get(key);
    const count = currentCount ? parseInt(currentCount) : 0;

    if (count >= maxRequests) {
      return c.json<ApiResponse>({
        success: false,
        error: {
          code: ErrorCodes.RATE_LIMITED,
          message: 'Too many requests for this operation',
        },
      }, 429, {
        'Retry-After': windowSec.toString(),
      });
    }

    await c.env.KV.put(key, (count + 1).toString(), {
      expirationTtl: windowSec,
    });

    await next();
  } catch (error) {
    console.error('Strict rate limit error:', error);
    await next();
  }
}
