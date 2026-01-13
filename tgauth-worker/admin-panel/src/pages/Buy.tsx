import { useState, useEffect, useRef } from 'react';
import QRCode from 'qrcode';

interface Product {
  id: string;
  name: string;
  description: string | null;
  price: number;
  key_type: 'time' | 'permanent';
  duration_days: number | null;
  max_devices: number;
}

interface OrderInfo {
  order_no: string;
  product_name: string;
  amount: number;
  qr_code: string;
  expires_at: number;
}

const API_BASE = import.meta.env.VITE_API_URL || '';

export default function Buy() {
  const [products, setProducts] = useState<Product[]>([]);
  const [loading, setLoading] = useState(true);
  const [selectedProduct, setSelectedProduct] = useState<Product | null>(null);
  const [order, setOrder] = useState<OrderInfo | null>(null);
  const [orderStatus, setOrderStatus] = useState<string>('');
  const [keyCode, setKeyCode] = useState<string>('');
  const [creating, setCreating] = useState(false);
  const [copied, setCopied] = useState(false);
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const pollRef = useRef<number | null>(null);

  useEffect(() => {
    loadProducts();
    return () => {
      if (pollRef.current) clearInterval(pollRef.current);
    };
  }, []);

  useEffect(() => {
    if (order?.qr_code && canvasRef.current) {
      QRCode.toCanvas(canvasRef.current, order.qr_code, { width: 200, margin: 2 });
    }
  }, [order]);

  const loadProducts = async () => {
    try {
      const res = await fetch(`${API_BASE}/api/pay/products`);
      const data = await res.json();
      if (data.success) {
        setProducts(data.data.products);
      }
    } catch (err) {
      console.error('Failed to load products:', err);
    } finally {
      setLoading(false);
    }
  };

  const createOrder = async (product: Product) => {
    setCreating(true);
    setSelectedProduct(product);
    setOrder(null);
    setOrderStatus('');
    setKeyCode('');

    try {
      const res = await fetch(`${API_BASE}/api/pay/create`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ product_id: product.id }),
      });
      const data = await res.json();
      
      if (data.success) {
        setOrder(data.data);
        setOrderStatus('pending');
        startPolling(data.data.order_no);
      } else {
        alert('创建订单失败');
        setSelectedProduct(null);
      }
    } catch (err) {
      console.error('Failed to create order:', err);
      alert('创建订单失败');
      setSelectedProduct(null);
    } finally {
      setCreating(false);
    }
  };

  const startPolling = (orderNo: string) => {
    if (pollRef.current) clearInterval(pollRef.current);
    
    pollRef.current = window.setInterval(async () => {
      try {
        const res = await fetch(`${API_BASE}/api/pay/status/${orderNo}`);
        const data = await res.json();
        
        if (data.success) {
          setOrderStatus(data.data.status);
          
          if (data.data.status === 'delivered') {
            setKeyCode(data.data.keyCode);
            if (pollRef.current) clearInterval(pollRef.current);
          } else if (data.data.status === 'expired') {
            if (pollRef.current) clearInterval(pollRef.current);
          }
        }
      } catch (err) {
        console.error('Failed to check status:', err);
      }
    }, 3000);
  };

  const copyKey = () => {
    navigator.clipboard.writeText(keyCode);
    setCopied(true);
    setTimeout(() => setCopied(false), 2000);
  };

  const resetOrder = () => {
    if (pollRef.current) clearInterval(pollRef.current);
    setSelectedProduct(null);
    setOrder(null);
    setOrderStatus('');
    setKeyCode('');
  };

  const formatCountdown = (expiresAt: number) => {
    const now = Math.floor(Date.now() / 1000);
    const remaining = expiresAt - now;
    if (remaining <= 0) return '已过期';
    const minutes = Math.floor(remaining / 60);
    const seconds = remaining % 60;
    return `${minutes}:${seconds.toString().padStart(2, '0')}`;
  };

  const [countdown, setCountdown] = useState('');
  useEffect(() => {
    if (!order) return;
    const timer = setInterval(() => {
      setCountdown(formatCountdown(order.expires_at));
    }, 1000);
    return () => clearInterval(timer);
  }, [order]);

  if (loading) {
    return (
      <div className="min-h-screen bg-gradient-to-b from-slate-50 to-slate-100 flex items-center justify-center">
        <svg className="animate-spin w-10 h-10 text-teal-500" fill="none" viewBox="0 0 24 24">
          <circle className="opacity-25" cx="12" cy="12" r="10" stroke="currentColor" strokeWidth="4"></circle>
          <path className="opacity-75" fill="currentColor" d="M4 12a8 8 0 018-8V0C5.373 0 0 5.373 0 12h4zm2 5.291A7.962 7.962 0 014 12H0c0 3.042 1.135 5.824 3 7.938l3-2.647z"></path>
        </svg>
      </div>
    );
  }

  // 支付成功页面
  if (orderStatus === 'delivered' && keyCode) {
    return (
      <div className="min-h-screen bg-gradient-to-b from-slate-50 to-slate-100 flex items-center justify-center p-4">
        <div className="bg-white rounded-2xl shadow-lg border border-slate-200 p-8 max-w-md w-full text-center">
          <div className="w-16 h-16 rounded-full bg-emerald-100 flex items-center justify-center mx-auto mb-4">
            <svg className="w-8 h-8 text-emerald-500" fill="none" viewBox="0 0 24 24" stroke="currentColor">
              <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M5 13l4 4L19 7" />
            </svg>
          </div>
          <h2 className="text-2xl font-bold text-slate-800 mb-2">支付成功</h2>
          <p className="text-slate-500 mb-6">您的卡密已生成，请妥善保管</p>
          
          <div className="bg-slate-50 rounded-xl p-4 mb-6 border border-slate-200">
            <p className="text-sm text-slate-500 mb-2">卡密</p>
            <code className="text-lg font-mono text-teal-600 break-all font-semibold">{keyCode}</code>
          </div>
          
          <button
            onClick={copyKey}
            className="w-full py-3 rounded-xl bg-teal-500 hover:bg-teal-600 text-white font-medium transition-colors mb-3"
          >
            {copied ? '已复制！' : '复制卡密'}
          </button>
          
          <button
            onClick={resetOrder}
            className="w-full py-3 rounded-xl bg-slate-100 hover:bg-slate-200 text-slate-700 font-medium transition-colors"
          >
            继续购买
          </button>
        </div>
      </div>
    );
  }

  // 支付页面
  if (order && selectedProduct) {
    return (
      <div className="min-h-screen bg-gradient-to-b from-slate-50 to-slate-100 flex items-center justify-center p-4">
        <div className="bg-white rounded-2xl shadow-lg border border-slate-200 p-6 max-w-sm w-full">
          <div className="text-center mb-6">
            <h2 className="text-xl font-bold text-slate-800 mb-1">{order.product_name}</h2>
            <p className="text-3xl font-bold text-teal-600">¥{order.amount}</p>
          </div>

          {orderStatus === 'expired' ? (
            <div className="text-center py-8">
              <div className="w-16 h-16 rounded-full bg-red-100 flex items-center justify-center mx-auto mb-4">
                <svg className="w-8 h-8 text-red-500" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                  <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M12 8v4l3 3m6-3a9 9 0 11-18 0 9 9 0 0118 0z" />
                </svg>
              </div>
              <p className="text-red-500 mb-4">订单已过期</p>
              <button onClick={resetOrder} className="px-6 py-2 rounded-lg bg-slate-100 hover:bg-slate-200 text-slate-700 transition-colors">
                重新下单
              </button>
            </div>
          ) : (
            <>
              <div className="bg-white rounded-xl p-4 mb-4 flex justify-center border border-slate-200">
                <canvas ref={canvasRef}></canvas>
              </div>
              
              <div className="text-center mb-4">
                <p className="text-slate-500 text-sm mb-1">请使用支付宝扫码支付</p>
                <p className="text-amber-600 text-sm font-medium">剩余时间: {countdown}</p>
              </div>

              <div className="flex items-center justify-center gap-2 text-slate-400 text-sm mb-4">
                <svg className="animate-spin w-4 h-4" fill="none" viewBox="0 0 24 24">
                  <circle className="opacity-25" cx="12" cy="12" r="10" stroke="currentColor" strokeWidth="4"></circle>
                  <path className="opacity-75" fill="currentColor" d="M4 12a8 8 0 018-8V0C5.373 0 0 5.373 0 12h4zm2 5.291A7.962 7.962 0 014 12H0c0 3.042 1.135 5.824 3 7.938l3-2.647z"></path>
                </svg>
                等待支付...
              </div>

              <button onClick={resetOrder} className="w-full py-2 rounded-lg bg-slate-100 hover:bg-slate-200 text-slate-500 text-sm transition-colors">
                取消订单
              </button>
            </>
          )}
        </div>
      </div>
    );
  }

  // 商品列表
  return (
    <div className="min-h-screen bg-gradient-to-b from-slate-50 to-slate-100 p-4 lg:p-8">
      <div className="max-w-4xl mx-auto">
        {/* Header */}
        <div className="text-center mb-8">
          <img src="/logo.png" alt="Logo" className="w-16 h-16 object-contain mx-auto mb-4" />
          <h1 className="text-2xl lg:text-3xl font-bold text-slate-800 mb-2">购买授权卡密</h1>
          <p className="text-slate-500">选择适合您的套餐</p>
        </div>

        {/* Products Grid */}
        <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 gap-4">
          {products.map((product, index) => (
            <div
              key={product.id}
              className={`bg-white rounded-2xl border-2 p-6 transition-all hover:shadow-lg ${
                index === 1 ? 'border-teal-500 shadow-lg shadow-teal-500/10' : 'border-slate-200 hover:border-teal-300'
              }`}
            >
              {index === 1 && (
                <div className="text-center mb-3">
                  <span className="inline-block px-3 py-1 rounded-full bg-teal-500 text-white text-xs font-medium">推荐</span>
                </div>
              )}
              
              <div className="text-center mb-4">
                <h3 className="text-xl font-bold text-slate-800 mb-1">{product.name}</h3>
                <p className="text-slate-500 text-sm">{product.description}</p>
              </div>
              
              <div className="text-center mb-4">
                <span className="text-3xl font-bold text-slate-800">¥{product.price}</span>
              </div>

              <div className="space-y-2 mb-6 text-sm">
                <div className="flex items-center gap-2 text-slate-600">
                  <svg className="w-4 h-4 text-teal-500" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                    <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M5 13l4 4L19 7" />
                  </svg>
                  {product.key_type === 'permanent' ? '永久有效' : `${product.duration_days}天有效期`}
                </div>
                <div className="flex items-center gap-2 text-slate-600">
                  <svg className="w-4 h-4 text-teal-500" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                    <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M5 13l4 4L19 7" />
                  </svg>
                  支持 {product.max_devices} 台设备
                </div>
                <div className="flex items-center gap-2 text-slate-600">
                  <svg className="w-4 h-4 text-teal-500" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                    <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M5 13l4 4L19 7" />
                  </svg>
                  即时发货
                </div>
              </div>

              <button
                onClick={() => createOrder(product)}
                disabled={creating}
                className={`w-full py-3 rounded-xl font-medium transition-all disabled:opacity-50 ${
                  index === 1
                    ? 'bg-teal-500 hover:bg-teal-600 text-white'
                    : 'bg-slate-100 hover:bg-slate-200 text-slate-700'
                }`}
              >
                {creating && selectedProduct?.id === product.id ? '创建订单...' : '立即购买'}
              </button>
            </div>
          ))}
        </div>

        {/* Footer */}
        <div className="text-center mt-8 text-slate-400 text-sm">
          <p>支付完成后卡密将自动发放</p>
          <p className="mt-1">如有问题请联系客服</p>
        </div>
      </div>
    </div>
  );
}
