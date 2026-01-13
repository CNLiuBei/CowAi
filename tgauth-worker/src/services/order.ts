// 订单服务
import { Env } from '../types';
import { AlipayService } from './alipay';
import { LicenseService } from './license';

export interface Product {
  id: string;
  name: string;
  description: string | null;
  price: number;
  key_type: 'time' | 'permanent';
  duration_days: number | null;
  max_devices: number;
  is_active: number;
  sort_order: number;
}

export interface Order {
  id: number;
  order_no: string;
  trade_no: string | null;
  product_id: string;
  product_name: string;
  amount: number;
  status: 'pending' | 'paid' | 'delivered' | 'expired' | 'refunded';
  buyer_id: string | null;
  key_code: string | null;
  qr_code: string | null;
  created_at: number;
  paid_at: number | null;
  delivered_at: number | null;
  expires_at: number;
}

// 生成订单号
function generateOrderNo(): string {
  const now = new Date();
  const dateStr = now.toISOString().replace(/[-:T.Z]/g, '').substring(0, 14);
  const random = Math.random().toString(36).substring(2, 8).toUpperCase();
  return `ORD${dateStr}${random}`;
}

export class OrderService {
  private env: Env;
  private alipay: AlipayService;
  private license: LicenseService;

  constructor(env: Env) {
    this.env = env;
    this.alipay = new AlipayService(env);
    this.license = new LicenseService(env);
  }

  // 获取商品列表
  async getProducts(): Promise<Product[]> {
    const result = await this.env.DB.prepare(
      'SELECT * FROM products WHERE is_active = 1 ORDER BY sort_order ASC, price ASC'
    ).all<Product>();
    return result.results || [];
  }

  // 获取单个商品
  async getProduct(productId: string): Promise<Product | null> {
    return await this.env.DB.prepare(
      'SELECT * FROM products WHERE id = ? AND is_active = 1'
    ).bind(productId).first<Product>();
  }

  // 创建订单
  async createOrder(productId: string, buyerId?: string): Promise<Order | null> {
    const product = await this.getProduct(productId);
    if (!product) {
      return null;
    }

    const orderNo = generateOrderNo();
    const now = Math.floor(Date.now() / 1000);
    const expiresAt = now + 15 * 60; // 15分钟过期

    // 创建支付宝预下单
    const alipayResult = await this.alipay.createQrPay(orderNo, product.price, product.name);
    if (!alipayResult) {
      return null;
    }

    // 保存订单
    await this.env.DB.prepare(
      `INSERT INTO orders (order_no, product_id, product_name, amount, buyer_id, qr_code, created_at, expires_at)
       VALUES (?, ?, ?, ?, ?, ?, ?, ?)`
    ).bind(orderNo, product.id, product.name, product.price, buyerId || null, alipayResult.qrCode, now, expiresAt).run();

    return await this.getOrder(orderNo);
  }

  // 获取订单
  async getOrder(orderNo: string): Promise<Order | null> {
    return await this.env.DB.prepare(
      'SELECT * FROM orders WHERE order_no = ?'
    ).bind(orderNo).first<Order>();
  }

  // 查询订单支付状态
  async checkOrderStatus(orderNo: string): Promise<{ status: string; keyCode?: string }> {
    const order = await this.getOrder(orderNo);
    if (!order) {
      return { status: 'not_found' };
    }

    // 已发货直接返回
    if (order.status === 'delivered') {
      return { status: 'delivered', keyCode: order.key_code || undefined };
    }

    // 已过期
    const now = Math.floor(Date.now() / 1000);
    if (order.status === 'pending' && order.expires_at < now) {
      await this.env.DB.prepare(
        'UPDATE orders SET status = ? WHERE order_no = ?'
      ).bind('expired', orderNo).run();
      return { status: 'expired' };
    }

    // 查询支付宝订单状态
    if (order.status === 'pending') {
      const alipayStatus = await this.alipay.queryOrder(orderNo);
      if (alipayStatus && alipayStatus.status === 'TRADE_SUCCESS') {
        // 支付成功，发货
        return await this.deliverOrder(orderNo, alipayStatus.tradeNo);
      }
    }

    return { status: order.status };
  }

  // 处理支付回调
  async handlePayNotify(params: Record<string, string>): Promise<boolean> {
    // 验签
    const isValid = await this.alipay.verifyNotify(params);
    if (!isValid) {
      console.error('Invalid alipay notify signature');
      return false;
    }

    const orderNo = params.out_trade_no;
    const tradeNo = params.trade_no;
    const tradeStatus = params.trade_status;

    if (tradeStatus !== 'TRADE_SUCCESS' && tradeStatus !== 'TRADE_FINISHED') {
      return true; // 非成功状态，返回成功但不处理
    }

    const order = await this.getOrder(orderNo);
    if (!order) {
      console.error('Order not found:', orderNo);
      return false;
    }

    if (order.status !== 'pending') {
      return true; // 已处理过
    }

    // 发货
    await this.deliverOrder(orderNo, tradeNo);
    return true;
  }

  // 发货（生成卡密）
  async deliverOrder(orderNo: string, tradeNo?: string): Promise<{ status: string; keyCode?: string }> {
    const order = await this.getOrder(orderNo);
    if (!order || order.status === 'delivered') {
      return { status: order?.status || 'not_found', keyCode: order?.key_code || undefined };
    }

    const product = await this.getProduct(order.product_id);
    if (!product) {
      return { status: 'error' };
    }

    const now = Math.floor(Date.now() / 1000);

    // 生成卡密
    const keys = await this.license.generateKeys({
      count: 1,
      keyType: product.key_type,
      durationDays: product.duration_days || undefined,
      maxDevices: product.max_devices,
      createdBy: 'payment',
      note: `订单: ${orderNo}`,
    });

    if (keys.length === 0) {
      return { status: 'error' };
    }

    const keyCode = keys[0];

    // 更新订单
    await this.env.DB.prepare(
      `UPDATE orders SET status = 'delivered', trade_no = ?, key_code = ?, paid_at = ?, delivered_at = ?
       WHERE order_no = ?`
    ).bind(tradeNo || null, keyCode, now, now, orderNo).run();

    return { status: 'delivered', keyCode };
  }

  // 获取订单统计
  async getOrderStats(): Promise<{ total: number; pending: number; paid: number; delivered: number; expired: number; totalAmount: number }> {
    const stats = await this.env.DB.prepare(`
      SELECT 
        COUNT(*) as total,
        SUM(CASE WHEN status = 'pending' THEN 1 ELSE 0 END) as pending,
        SUM(CASE WHEN status = 'paid' THEN 1 ELSE 0 END) as paid,
        SUM(CASE WHEN status = 'delivered' THEN 1 ELSE 0 END) as delivered,
        SUM(CASE WHEN status = 'expired' THEN 1 ELSE 0 END) as expired,
        SUM(CASE WHEN status = 'delivered' THEN amount ELSE 0 END) as total_amount
      FROM orders
    `).first<{ total: number; pending: number; paid: number; delivered: number; expired: number; total_amount: number }>();

    return {
      total: stats?.total || 0,
      pending: stats?.pending || 0,
      paid: stats?.paid || 0,
      delivered: stats?.delivered || 0,
      expired: stats?.expired || 0,
      totalAmount: stats?.total_amount || 0,
    };
  }
}
