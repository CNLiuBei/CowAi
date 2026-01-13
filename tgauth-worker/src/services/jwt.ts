import { Env, JWTPayload } from '../types';

export class JWTService {
  private env: Env;

  constructor(env: Env) {
    this.env = env;
  }

  // 生成 JWT Token
  async generateToken(params: {
    tg_user_id: string;
    device_id: string;
  }): Promise<{ token: string; expires_at: number }> {
    const now = Math.floor(Date.now() / 1000);
    const expireDays = parseInt(this.env.TOKEN_EXPIRE_DAYS) || 7;
    const exp = now + expireDays * 24 * 60 * 60;

    const payload: JWTPayload = {
      sub: params.tg_user_id,
      did: params.device_id,
      iat: now,
      exp: exp,
      jti: crypto.randomUUID(),
    };

    const token = await this.sign(payload);
    return { token, expires_at: exp };
  }

  // 验证 JWT Token
  async verifyToken(token: string): Promise<JWTPayload | null> {
    try {
      const parts = token.split('.');
      if (parts.length !== 3) {
        return null;
      }

      const [headerB64, payloadB64, signatureB64] = parts;

      // 验证签名
      const data = `${headerB64}.${payloadB64}`;
      const signature = this.base64UrlDecode(signatureB64);
      const isValid = await this.verifySignature(data, signature);

      if (!isValid) {
        return null;
      }

      // 解码 payload
      const payloadJson = this.base64UrlDecode(payloadB64);
      const payload = JSON.parse(new TextDecoder().decode(payloadJson)) as JWTPayload;

      // 检查过期
      const now = Math.floor(Date.now() / 1000);
      if (payload.exp < now) {
        return null;
      }

      return payload;
    } catch (error) {
      console.error('Error verifying token:', error);
      return null;
    }
  }

  // 签名 JWT
  private async sign(payload: JWTPayload): Promise<string> {
    const header = { alg: 'HS256', typ: 'JWT' };
    
    const headerB64 = this.base64UrlEncode(
      new TextEncoder().encode(JSON.stringify(header))
    );
    const payloadB64 = this.base64UrlEncode(
      new TextEncoder().encode(JSON.stringify(payload))
    );

    const data = `${headerB64}.${payloadB64}`;
    const signature = await this.createSignature(data);
    const signatureB64 = this.base64UrlEncode(signature);

    return `${data}.${signatureB64}`;
  }

  // 创建 HMAC-SHA256 签名
  private async createSignature(data: string): Promise<Uint8Array> {
    const key = await crypto.subtle.importKey(
      'raw',
      new TextEncoder().encode(this.env.JWT_SECRET),
      { name: 'HMAC', hash: 'SHA-256' },
      false,
      ['sign']
    );

    const signature = await crypto.subtle.sign(
      'HMAC',
      key,
      new TextEncoder().encode(data)
    );

    return new Uint8Array(signature);
  }

  // 验证签名
  private async verifySignature(data: string, signature: Uint8Array): Promise<boolean> {
    const key = await crypto.subtle.importKey(
      'raw',
      new TextEncoder().encode(this.env.JWT_SECRET),
      { name: 'HMAC', hash: 'SHA-256' },
      false,
      ['verify']
    );

    return await crypto.subtle.verify(
      'HMAC',
      key,
      signature,
      new TextEncoder().encode(data)
    );
  }

  // Base64 URL 编码
  private base64UrlEncode(data: Uint8Array): string {
    const base64 = btoa(String.fromCharCode(...data));
    return base64
      .replace(/\+/g, '-')
      .replace(/\//g, '_')
      .replace(/=+$/, '');
  }

  // Base64 URL 解码
  private base64UrlDecode(str: string): Uint8Array {
    let base64 = str.replace(/-/g, '+').replace(/_/g, '/');
    while (base64.length % 4) {
      base64 += '=';
    }
    const binary = atob(base64);
    const bytes = new Uint8Array(binary.length);
    for (let i = 0; i < binary.length; i++) {
      bytes[i] = binary.charCodeAt(i);
    }
    return bytes;
  }
}
