// 支付路由
import { Hono } from 'hono';
import { Env, ApiResponse, ErrorCodes } from '../types';
import { OrderService } from '../services/order';

export const payRoutes = new Hono<{ Bindings: Env }>();

// GET /api/pay/products - 获取商品列表
payRoutes.get('/products', async (c) => {
  const orderService = new OrderService(c.env);
  
  try {
    const products = await orderService.getProducts();
    return c.json<ApiResponse>({
      success: true,
      data: { products },
    });
  } catch (error) {
    console.error('Error getting products:', error);
    return c.json<ApiResponse>({
      success: false,
      error: { code: ErrorCodes.INTERNAL_ERROR, message: 'Failed to get products' },
    }, 500);
  }
});

// POST /api/pay/create - 创建订单
payRoutes.post('/create', async (c) => {
  const body = await c.req.json<{ product_id: string; buyer_id?: string }>().catch(() => null);
  
  if (!body?.product_id) {
    return c.json<ApiResponse>({
      success: false,
      error: { code: ErrorCodes.BAD_REQUEST, message: 'product_id is required' },
    }, 400);
  }

  const orderService = new OrderService(c.env);
  
  try {
    const order = await orderService.createOrder(body.product_id, body.buyer_id);
    
    if (!order) {
      return c.json<ApiResponse>({
        success: false,
        error: { code: ErrorCodes.BAD_REQUEST, message: 'Failed to create order' },
      }, 400);
    }

    return c.json<ApiResponse>({
      success: true,
      data: {
        order_no: order.order_no,
        product_name: order.product_name,
        amount: order.amount,
        qr_code: order.qr_code,
        expires_at: order.expires_at,
      },
    });
  } catch (error) {
    console.error('Error creating order:', error);
    return c.json<ApiResponse>({
      success: false,
      error: { code: ErrorCodes.INTERNAL_ERROR, message: 'Failed to create order' },
    }, 500);
  }
});

// GET /api/pay/status/:orderNo - 查询订单状态
payRoutes.get('/status/:orderNo', async (c) => {
  const orderNo = c.req.param('orderNo');
  const orderService = new OrderService(c.env);
  
  try {
    const result = await orderService.checkOrderStatus(orderNo);
    
    return c.json<ApiResponse>({
      success: true,
      data: result,
    });
  } catch (error) {
    console.error('Error checking order status:', error);
    return c.json<ApiResponse>({
      success: false,
      error: { code: ErrorCodes.INTERNAL_ERROR, message: 'Failed to check order status' },
    }, 500);
  }
});

// POST /api/pay/notify - 支付宝异步通知
payRoutes.post('/notify', async (c) => {
  const orderService = new OrderService(c.env);
  
  try {
    // 解析 form 数据
    const formData = await c.req.formData();
    const params: Record<string, string> = {};
    formData.forEach((value, key) => {
      params[key] = value.toString();
    });

    console.log('Alipay notify params:', JSON.stringify(params));

    const success = await orderService.handlePayNotify(params);
    
    // 支付宝要求返回 "success" 字符串
    return c.text(success ? 'success' : 'fail');
  } catch (error) {
    console.error('Error handling pay notify:', error);
    return c.text('fail');
  }
});

// GET /api/pay/order/:orderNo - 获取订单详情
payRoutes.get('/order/:orderNo', async (c) => {
  const orderNo = c.req.param('orderNo');
  const orderService = new OrderService(c.env);
  
  try {
    const order = await orderService.getOrder(orderNo);
    
    if (!order) {
      return c.json<ApiResponse>({
        success: false,
        error: { code: ErrorCodes.NOT_FOUND, message: 'Order not found' },
      }, 404);
    }

    return c.json<ApiResponse>({
      success: true,
      data: {
        order_no: order.order_no,
        product_name: order.product_name,
        amount: order.amount,
        status: order.status,
        key_code: order.status === 'delivered' ? order.key_code : null,
        qr_code: order.status === 'pending' ? order.qr_code : null,
        created_at: order.created_at,
        expires_at: order.expires_at,
      },
    });
  } catch (error) {
    console.error('Error getting order:', error);
    return c.json<ApiResponse>({
      success: false,
      error: { code: ErrorCodes.INTERNAL_ERROR, message: 'Failed to get order' },
    }, 500);
  }
});
