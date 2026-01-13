import axios from 'axios';

// 使用相对路径，这样前端和后端在同一个域名下
const API_BASE = import.meta.env.VITE_API_URL || '';

const api = axios.create({
  baseURL: API_BASE,
  headers: {
    'Content-Type': 'application/json',
  },
});

// 添加请求拦截器
api.interceptors.request.use((config) => {
  const secret = localStorage.getItem('admin_secret');
  if (secret) {
    config.headers['X-Bot-Secret'] = secret;
  }
  return config;
});

export interface KeyStats {
  total: number;
  unused: number;
  activated: number;
  expired: number;
  revoked: number;
}

export interface LicenseKey {
  id: number;
  key_code: string;
  key_type: 'time' | 'permanent';
  duration_days: number | null;
  max_devices: number;
  created_by: string;
  created_at: number;
  activated_by: string | null;
  activated_at: number | null;
  expires_at: number | null;
  status: 'unused' | 'activated' | 'expired' | 'revoked';
  note: string | null;
}

export interface SystemStats {
  total_users: number;
  active_devices: number;
  pending_sessions: number;
  timestamp: number;
}

// 卡密统计
export const getKeyStats = () => api.get<{ success: boolean; data: KeyStats }>('/api/admin/key_stats');

// 卡密列表
export const getKeys = (params?: { status?: string; limit?: number; offset?: number }) =>
  api.get<{ success: boolean; data: { keys: LicenseKey[]; total: number } }>('/api/admin/keys', { params });

// 生成卡密
export const generateKeys = (data: {
  count: number;
  key_type: 'time' | 'permanent';
  duration_days?: number;
  max_devices?: number;
  created_by: string;
  note?: string;
}) => api.post<{ success: boolean; data: { keys: string[]; count: number } }>('/api/admin/generate_keys', data);

// 撤销卡密
export const revokeKey = (key_code: string) =>
  api.post<{ success: boolean }>('/api/admin/revoke_key', { key_code });

// 设备信息
export interface KeyDevice {
  device_id: string;
  bound_at: number;
  last_seen: number;
  is_active: number;
}

// 获取卡密详情 (包含设备列表)
export const getKeyDetails = (key_code: string) =>
  api.get<{ success: boolean; data: { key: LicenseKey; devices: KeyDevice[] } }>(`/api/admin/key/${key_code}`);

// 解绑设备
export const unbindDevice = (key_code: string, device_id: string) =>
  api.post<{ success: boolean }>(`/api/admin/key/${key_code}/unbind`, { device_id });

// 系统统计
export const getSystemStats = () =>
  api.get<{ success: boolean; data: SystemStats }>('/api/admin/stats');

// 封禁用户
export const banUser = (tg_user_id: string) =>
  api.post<{ success: boolean }>('/api/admin/ban', { tg_user_id });

// 解封用户
export const unbanUser = (tg_user_id: string) =>
  api.post<{ success: boolean }>('/api/admin/unban', { tg_user_id });

// 商品/套餐类型
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
  created_at: number;
}

// 获取商品列表 (管理员)
export const getProducts = () =>
  api.get<{ success: boolean; data: { products: Product[] } }>('/api/admin/products');

// 创建商品
export const createProduct = (data: {
  id: string;
  name: string;
  description?: string | null;
  price: number;
  key_type: 'time' | 'permanent';
  duration_days?: number | null;
  max_devices: number;
  sort_order?: number;
}) => api.post<{ success: boolean }>('/api/admin/products', data);

// 更新商品
export const updateProduct = (id: string, data: Partial<{
  name: string;
  description: string | null;
  price: number;
  key_type: 'time' | 'permanent';
  duration_days: number | null;
  max_devices: number;
  is_active: number;
  sort_order: number;
}>) => api.put<{ success: boolean }>(`/api/admin/products/${id}`, data);

// 删除商品
export const deleteProduct = (id: string) =>
  api.delete<{ success: boolean }>(`/api/admin/products/${id}`);

export default api;
