// 支付宝当面付服务
import { Env } from '../types';

// 支付宝网关
const ALIPAY_GATEWAY = 'https://openapi.alipay.com/gateway.do';

// RSA2 签名
async function signWithRSA2(content: string, privateKey: string): Promise<string> {
  // 将 PEM 格式私钥转换为 CryptoKey
  const pemHeader = '-----BEGIN PRIVATE KEY-----';
  const pemFooter = '-----END PRIVATE KEY-----';
  const pemContents = privateKey.replace(/\s/g, '');
  
  const binaryDer = Uint8Array.from(atob(pemContents), c => c.charCodeAt(0));
  
  const cryptoKey = await crypto.subtle.importKey(
    'pkcs8',
    binaryDer,
    { name: 'RSASSA-PKCS1-v1_5', hash: 'SHA-256' },
    false,
    ['sign']
  );
  
  const encoder = new TextEncoder();
  const data = encoder.encode(content);
  const signature = await crypto.subtle.sign('RSASSA-PKCS1-v1_5', cryptoKey, data);
  
  return btoa(String.fromCharCode(...new Uint8Array(signature)));
}

// RSA2 验签
async function verifyWithRSA2(content: string, sign: string, publicKey: string): Promise<boolean> {
  try {
    const pemContents = publicKey.replace(/\s/g, '');
    const binaryDer = Uint8Array.from(atob(pemContents), c => c.charCodeAt(0));
    
    const cryptoKey = await crypto.subtle.importKey(
      'spki',
      binaryDer,
      { name: 'RSASSA-PKCS1-v1_5', hash: 'SHA-256' },
      false,
      ['verify']
    );
    
    const encoder = new TextEncoder();
    const data = encoder.encode(content);
    const signatureBytes = Uint8Array.from(atob(sign), c => c.charCodeAt(0));
    
    return await crypto.subtle.verify('RSASSA-PKCS1-v1_5', cryptoKey, signatureBytes, data);
  } catch (e) {
    console.error('Verify signature error:', e);
    return false;
  }
}

// 构建签名字符串
function buildSignContent(params: Record<string, string>): string {
  const sortedKeys = Object.keys(params).sort();
  const pairs = sortedKeys
    .filter(key => params[key] !== '' && params[key] !== undefined && key !== 'sign')
    .map(key => `${key}=${params[key]}`);
  return pairs.join('&');
}

export class AlipayService {
  private appId: string;
  private privateKey: string;
  private alipayPublicKey: string;
  private notifyUrl: string;

  constructor(env: Env) {
    this.appId = env.ALIPAY_APP_ID;
    this.privateKey = env.ALIPAY_PRIVATE_KEY;
    this.alipayPublicKey = env.ALIPAY_PUBLIC_KEY;
    this.notifyUrl = env.ALIPAY_NOTIFY_URL || `https://cowai.trueliu.com/api/pay/notify`;
  }

  // 创建当面付预下单
  async createQrPay(orderNo: string, amount: number, subject: string): Promise<{ qrCode: string } | null> {
    const bizContent = {
      out_trade_no: orderNo,
      total_amount: amount.toFixed(2),
      subject: subject,
    };

    const params: Record<string, string> = {
      app_id: this.appId,
      method: 'alipay.trade.precreate',
      format: 'JSON',
      charset: 'utf-8',
      sign_type: 'RSA2',
      timestamp: new Date().toISOString().replace('T', ' ').substring(0, 19),
      version: '1.0',
      notify_url: this.notifyUrl,
      biz_content: JSON.stringify(bizContent),
    };

    // 签名
    const signContent = buildSignContent(params);
    params.sign = await signWithRSA2(signContent, this.privateKey);

    // 发送请求
    const formData = new URLSearchParams(params);
    const response = await fetch(ALIPAY_GATEWAY, {
      method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded;charset=utf-8' },
      body: formData.toString(),
    });

    const result = await response.json() as any;
    console.log('Alipay precreate response:', JSON.stringify(result));

    const precreateResponse = result.alipay_trade_precreate_response;
    if (precreateResponse && precreateResponse.code === '10000') {
      return { qrCode: precreateResponse.qr_code };
    }

    console.error('Alipay precreate failed:', precreateResponse);
    return null;
  }

  // 查询订单状态
  async queryOrder(orderNo: string): Promise<{ status: string; tradeNo?: string } | null> {
    const bizContent = {
      out_trade_no: orderNo,
    };

    const params: Record<string, string> = {
      app_id: this.appId,
      method: 'alipay.trade.query',
      format: 'JSON',
      charset: 'utf-8',
      sign_type: 'RSA2',
      timestamp: new Date().toISOString().replace('T', ' ').substring(0, 19),
      version: '1.0',
      biz_content: JSON.stringify(bizContent),
    };

    const signContent = buildSignContent(params);
    params.sign = await signWithRSA2(signContent, this.privateKey);

    const formData = new URLSearchParams(params);
    const response = await fetch(ALIPAY_GATEWAY, {
      method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded;charset=utf-8' },
      body: formData.toString(),
    });

    const result = await response.json() as any;
    const queryResponse = result.alipay_trade_query_response;

    if (queryResponse && queryResponse.code === '10000') {
      return {
        status: queryResponse.trade_status,
        tradeNo: queryResponse.trade_no,
      };
    }

    return null;
  }

  // 验证异步通知签名
  async verifyNotify(params: Record<string, string>): Promise<boolean> {
    const sign = params.sign;
    const signType = params.sign_type;

    if (!sign || signType !== 'RSA2') {
      return false;
    }

    // 移除 sign 和 sign_type 后构建验签字符串
    const verifyParams = { ...params };
    delete verifyParams.sign;
    delete verifyParams.sign_type;

    const signContent = buildSignContent(verifyParams);
    return await verifyWithRSA2(signContent, sign, this.alipayPublicKey);
  }
}
