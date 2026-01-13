import { useState } from 'react';
import { getKeyStats } from '../api';

interface LoginProps {
  onLogin: () => void;
}

export default function Login({ onLogin }: LoginProps) {
  const [secret, setSecret] = useState('');
  const [error, setError] = useState('');
  const [loading, setLoading] = useState(false);

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault();
    setError('');
    setLoading(true);

    localStorage.setItem('admin_secret', secret);

    try {
      await getKeyStats();
      onLogin();
    } catch {
      localStorage.removeItem('admin_secret');
      setError('密钥无效，请重试');
    } finally {
      setLoading(false);
    }
  };

  return (
    <div className="min-h-screen bg-gradient-to-b from-slate-50 to-slate-100 flex items-center justify-center p-4">
      <div className="w-full max-w-md">
        <div className="bg-white rounded-2xl border border-slate-200 p-8 shadow-lg">
          {/* Logo */}
          <div className="flex justify-center mb-8">
            <img src="/logo.png" alt="Logo" className="w-20 h-20 object-contain" />
          </div>

          <h2 className="text-2xl font-bold text-slate-800 text-center mb-2">卡密管理后台</h2>
          <p className="text-slate-500 text-center mb-8">请输入管理员密钥登录</p>

          <form onSubmit={handleSubmit} className="space-y-6">
            <div>
              <label className="block text-sm font-medium text-slate-700 mb-2">管理员密钥</label>
              <input
                type="password"
                value={secret}
                onChange={(e) => setSecret(e.target.value)}
                placeholder="请输入 Bot Secret"
                className="w-full px-4 py-3 rounded-xl bg-slate-50 border border-slate-200 text-slate-800 placeholder-slate-400 focus:outline-none focus:ring-2 focus:ring-teal-500 focus:border-transparent transition-all"
                required
              />
            </div>

            {error && (
              <div className="flex items-center gap-2 text-red-500 text-sm bg-red-50 px-4 py-3 rounded-lg border border-red-200">
                <svg className="w-5 h-5" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                  <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M12 8v4m0 4h.01M21 12a9 9 0 11-18 0 9 9 0 0118 0z" />
                </svg>
                {error}
              </div>
            )}

            <button
              type="submit"
              disabled={loading}
              className="w-full py-3 px-4 rounded-xl bg-teal-500 text-white font-medium hover:bg-teal-600 focus:outline-none focus:ring-2 focus:ring-teal-500 focus:ring-offset-2 transition-all disabled:opacity-50 disabled:cursor-not-allowed cursor-pointer"
            >
              {loading ? (
                <span className="flex items-center justify-center gap-2">
                  <svg className="animate-spin w-5 h-5" fill="none" viewBox="0 0 24 24">
                    <circle className="opacity-25" cx="12" cy="12" r="10" stroke="currentColor" strokeWidth="4"></circle>
                    <path className="opacity-75" fill="currentColor" d="M4 12a8 8 0 018-8V0C5.373 0 0 5.373 0 12h4zm2 5.291A7.962 7.962 0 014 12H0c0 3.042 1.135 5.824 3 7.938l3-2.647z"></path>
                  </svg>
                  验证中...
                </span>
              ) : '登录'}
            </button>
          </form>
        </div>
      </div>
    </div>
  );
}
