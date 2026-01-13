import { useState, useEffect } from 'react';
import { getKeyStats, getSystemStats } from '../api';
import type { KeyStats, SystemStats } from '../api';

export default function Dashboard() {
  const [keyStats, setKeyStats] = useState<KeyStats | null>(null);
  const [sysStats, setSysStats] = useState<SystemStats | null>(null);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    loadStats();
  }, []);

  const loadStats = async () => {
    try {
      const [keyRes, sysRes] = await Promise.all([
        getKeyStats(),
        getSystemStats()
      ]);
      if (keyRes.data.success) setKeyStats(keyRes.data.data);
      if (sysRes.data.success) setSysStats(sysRes.data.data);
    } catch (err) {
      console.error('Failed to load stats:', err);
    } finally {
      setLoading(false);
    }
  };

  const StatCard = ({ title, value, icon, color }: { title: string; value: number; icon: React.ReactNode; color: string }) => (
    <div className="card p-3 lg:p-6 hover:shadow-md transition-shadow">
      <div className="flex items-center lg:items-start gap-3 lg:justify-between">
        <div className={`p-2 lg:p-3 rounded-lg lg:rounded-xl ${color.replace('text-', 'bg-').replace('-600', '-100')} dark:bg-opacity-20 shrink-0 lg:order-2`}>
          {icon}
        </div>
        <div className="lg:order-1">
          <p className="text-xs lg:text-sm font-medium text-slate-500 dark:text-slate-400">{title}</p>
          <p className={`text-xl lg:text-3xl font-bold mt-0.5 lg:mt-2 ${color}`}>{loading ? '-' : value}</p>
        </div>
      </div>
    </div>
  );

  return (
    <div className="space-y-6 lg:space-y-8">
      {/* Page Header */}
      <div>
        <h1 className="text-xl lg:text-2xl font-bold text-slate-900 dark:text-white">仪表盘</h1>
        <p className="text-sm text-slate-500 dark:text-slate-400 mt-1">查看卡密和系统统计数据</p>
      </div>

      {/* Key Stats */}
      <div>
        <h2 className="text-base lg:text-lg font-semibold text-slate-900 dark:text-white mb-3 lg:mb-4">卡密统计</h2>
        <div className="grid grid-cols-2 sm:grid-cols-3 lg:grid-cols-5 gap-3 lg:gap-4">
          <StatCard
            title="总卡密数"
            value={keyStats?.total || 0}
            color="text-slate-600"
            icon={<svg className="w-6 h-6 text-slate-600" fill="none" viewBox="0 0 24 24" stroke="currentColor"><path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M15 7a2 2 0 012 2m4 0a6 6 0 01-7.743 5.743L11 17H9v2H7v2H4a1 1 0 01-1-1v-2.586a1 1 0 01.293-.707l5.964-5.964A6 6 0 1121 9z" /></svg>}
          />
          <StatCard
            title="未使用"
            value={keyStats?.unused || 0}
            color="text-blue-600"
            icon={<svg className="w-6 h-6 text-blue-600" fill="none" viewBox="0 0 24 24" stroke="currentColor"><path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M12 8v4l3 3m6-3a9 9 0 11-18 0 9 9 0 0118 0z" /></svg>}
          />
          <StatCard
            title="已激活"
            value={keyStats?.activated || 0}
            color="text-emerald-600"
            icon={<svg className="w-6 h-6 text-emerald-600" fill="none" viewBox="0 0 24 24" stroke="currentColor"><path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M9 12l2 2 4-4m6 2a9 9 0 11-18 0 9 9 0 0118 0z" /></svg>}
          />
          <StatCard
            title="已过期"
            value={keyStats?.expired || 0}
            color="text-amber-600"
            icon={<svg className="w-6 h-6 text-amber-600" fill="none" viewBox="0 0 24 24" stroke="currentColor"><path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M12 9v2m0 4h.01m-6.938 4h13.856c1.54 0 2.502-1.667 1.732-3L13.732 4c-.77-1.333-2.694-1.333-3.464 0L3.34 16c-.77 1.333.192 3 1.732 3z" /></svg>}
          />
          <StatCard
            title="已撤销"
            value={keyStats?.revoked || 0}
            color="text-red-600"
            icon={<svg className="w-6 h-6 text-red-600" fill="none" viewBox="0 0 24 24" stroke="currentColor"><path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M18.364 18.364A9 9 0 005.636 5.636m12.728 12.728A9 9 0 015.636 5.636m12.728 12.728L5.636 5.636" /></svg>}
          />
        </div>
      </div>

      {/* System Stats */}
      <div>
        <h2 className="text-base lg:text-lg font-semibold text-slate-900 dark:text-white mb-3 lg:mb-4">系统统计</h2>
        <div className="grid grid-cols-2 sm:grid-cols-3 gap-3 lg:gap-4">
          <StatCard
            title="总用户数"
            value={sysStats?.total_users || 0}
            color="text-teal-600"
            icon={<svg className="w-6 h-6 text-teal-600" fill="none" viewBox="0 0 24 24" stroke="currentColor"><path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M12 4.354a4 4 0 110 5.292M15 21H3v-1a6 6 0 0112 0v1zm0 0h6v-1a6 6 0 00-9-5.197M13 7a4 4 0 11-8 0 4 4 0 018 0z" /></svg>}
          />
          <StatCard
            title="活跃设备"
            value={sysStats?.active_devices || 0}
            color="text-cyan-600"
            icon={<svg className="w-6 h-6 text-cyan-600" fill="none" viewBox="0 0 24 24" stroke="currentColor"><path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M9.75 17L9 20l-1 1h8l-1-1-.75-3M3 13h18M5 17h14a2 2 0 002-2V5a2 2 0 00-2-2H5a2 2 0 00-2 2v10a2 2 0 002 2z" /></svg>}
          />
          <StatCard
            title="待处理会话"
            value={sysStats?.pending_sessions || 0}
            color="text-violet-600"
            icon={<svg className="w-6 h-6 text-violet-600" fill="none" viewBox="0 0 24 24" stroke="currentColor"><path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M8 12h.01M12 12h.01M16 12h.01M21 12c0 4.418-4.03 8-9 8a9.863 9.863 0 01-4.255-.949L3 20l1.395-3.72C3.512 15.042 3 13.574 3 12c0-4.418 4.03-8 9-8s9 3.582 9 8z" /></svg>}
          />
        </div>
      </div>

      {/* Quick Actions */}
      <div className="card p-4 lg:p-6">
        <h2 className="text-base lg:text-lg font-semibold text-slate-900 dark:text-white mb-3 lg:mb-4">快捷操作</h2>
        <div className="flex flex-col sm:flex-row gap-2 sm:gap-3">
          <a href="/cowadmin/generate" className="btn btn-primary flex items-center justify-center gap-2 py-2.5">
            <svg className="w-5 h-5" fill="none" viewBox="0 0 24 24" stroke="currentColor">
              <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M12 6v6m0 0v6m0-6h6m-6 0H6" />
            </svg>
            生成新卡密
          </a>
          <a href="/cowadmin/keys" className="btn btn-ghost flex items-center justify-center gap-2 py-2.5">
            <svg className="w-5 h-5" fill="none" viewBox="0 0 24 24" stroke="currentColor">
              <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M9 5H7a2 2 0 00-2 2v12a2 2 0 002 2h10a2 2 0 002-2V7a2 2 0 00-2-2h-2M9 5a2 2 0 002 2h2a2 2 0 002-2M9 5a2 2 0 012-2h2a2 2 0 012 2" />
            </svg>
            查看卡密列表
          </a>
          <button onClick={loadStats} className="btn btn-ghost flex items-center justify-center gap-2 py-2.5">
            <svg className="w-5 h-5" fill="none" viewBox="0 0 24 24" stroke="currentColor">
              <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M4 4v5h.582m15.356 2A8.001 8.001 0 004.582 9m0 0H9m11 11v-5h-.581m0 0a8.003 8.003 0 01-15.357-2m15.357 2H15" />
            </svg>
            刷新数据
          </button>
        </div>
      </div>
    </div>
  );
}
