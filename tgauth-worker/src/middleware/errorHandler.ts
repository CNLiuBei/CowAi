import { Context, Next } from 'hono';
import { Env, ApiResponse, ErrorCodes } from '../types';

// 自定义错误类
export class AppError extends Error {
  code: string;
  statusCode: number;

  constructor(code: string, message: string, statusCode: number = 400) {
    super(message);
    this.code = code;
    this.statusCode = statusCode;
    this.name = 'AppError';
  }
}

// 错误处理中间件
export async function errorHandler(c: Context<{ Bindings: Env }>, next: Next) {
  try {
    await next();
  } catch (error) {
    console.error('Error caught by middleware:', error);

    if (error instanceof AppError) {
      return c.json<ApiResponse>({
        success: false,
        error: {
          code: error.code,
          message: error.message,
        },
      }, error.statusCode);
    }

    // 处理 JSON 解析错误
    if (error instanceof SyntaxError && 'body' in error) {
      return c.json<ApiResponse>({
        success: false,
        error: {
          code: ErrorCodes.BAD_REQUEST,
          message: 'Invalid JSON in request body',
        },
      }, 400);
    }

    // 未知错误
    return c.json<ApiResponse>({
      success: false,
      error: {
        code: ErrorCodes.INTERNAL_ERROR,
        message: 'An unexpected error occurred',
      },
    }, 500);
  }
}

// 请求验证中间件
export async function validateRequest(c: Context<{ Bindings: Env }>, next: Next) {
  // 检查 Content-Type (对于 POST/PUT 请求)
  const method = c.req.method;
  if (method === 'POST' || method === 'PUT') {
    const contentType = c.req.header('Content-Type');
    if (!contentType?.includes('application/json')) {
      return c.json<ApiResponse>({
        success: false,
        error: {
          code: ErrorCodes.BAD_REQUEST,
          message: 'Content-Type must be application/json',
        },
      }, 400);
    }
  }

  await next();
}
