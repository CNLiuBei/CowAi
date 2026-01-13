import { useState, useEffect, lazy, Suspense } from 'react';
import { BrowserRouter, Routes, Route, NavLink, Navigate, useLocation } from 'react-router-dom';

// 懒加载页面组件
const Dashboard = lazy(() => import('./pages/Dashboard'));
const Keys = lazy(() => import('./pages/Keys'));
const Generate = lazy(() => import('./pages/Generate'));
const Products = lazy(() => import('./pages/Products'));
const Login = lazy(() => import('./pages/Login'));
const Buy = lazy(() => import('./pages/Buy'));

// 加载中组件
const Loading = () => (
  <div className="flex items-center justify-center min-h-[200px]">
    <svg className="animate-spin w-8 h-8 text-teal-500" fill="none" viewBox="0 0 24 24">
      <circle className="opacity-25" cx="12" cy="12" r="10" stroke="currentColor" strokeWidth="4"></circle>
      <path className="opacity-75" fill="currentColor" d="M4 12a8 8 0 018-8V0C5.373 0 0 5.373 0 12h4zm2 5.291A7.962 7.962 0 014 12H0c0 3.042 1.135 5.824 3 7.938l3-2.647z"></path>
    </svg>
  </div>
);

// 管理后台布局
function AdminLayout({ children, onLogout }: { children: React.ReactNode; onLogout: () => void }) {
  const [sidebarOpen, setSidebarOpen] = useState(false);

  const handleNavClick = () => {
    if (window.innerWidth < 1024) setSidebarOpen(false);
  };

  return (
    <div className="min-h-screen bg-slate-50 dark:bg-slate-900">
      {sidebarOpen && <div className="fixed inset-0 bg-black/50 z-30 lg:hidden" onClick={() => setSidebarOpen(false)} />}

      <aside className={`fixed top-0 left-0 z-40 h-screen transition-transform lg:translate-x-0 ${sidebarOpen ? 'translate-x-0' : '-translate-x-full'} bg-white dark:bg-slate-800 border-r border-slate-200 dark:border-slate-700 w-64`}>
        <div className="flex flex-col h-full">
          <div className="flex items-center justify-between px-4 py-4 border-b border-slate-200 dark:border-slate-700">
            <div className="flex items-center gap-3">
              <img src="/logo.png" alt="Logo" className="w-10 h-10 object-contain" />
              <div>
                <h1 className="font-bold text-slate-900 dark:text-white">卡密管理</h1>
                <p className="text-xs text-slate-500">Admin Panel</p>
              </div>
            </div>
            <button onClick={() => setSidebarOpen(false)} className="lg:hidden p-2 rounded-lg hover:bg-slate-100 dark:hover:bg-slate-700">
              <svg className="w-5 h-5 text-slate-500" fill="none" viewBox="0 0 24 24" stroke="currentColor"><path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M6 18L18 6M6 6l12 12" /></svg>
            </button>
          </div>

          <nav className="flex-1 px-3 py-4 space-y-1">
            <NavLink to="/cowadmin" end onClick={handleNavClick} className={({isActive}) => `flex items-center gap-3 px-4 py-3 rounded-lg transition-colors cursor-pointer ${isActive ? 'bg-teal-50 dark:bg-teal-900/30 text-teal-600 dark:text-teal-400' : 'text-slate-600 dark:text-slate-400 hover:bg-slate-100 dark:hover:bg-slate-700'}`}>
              <svg className="w-5 h-5" fill="none" viewBox="0 0 24 24" stroke="currentColor"><path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M4 6a2 2 0 012-2h2a2 2 0 012 2v2a2 2 0 01-2 2H6a2 2 0 01-2-2V6zM14 6a2 2 0 012-2h2a2 2 0 012 2v2a2 2 0 01-2 2h-2a2 2 0 01-2-2V6zM4 16a2 2 0 012-2h2a2 2 0 012 2v2a2 2 0 01-2 2H6a2 2 0 01-2-2v-2zM14 16a2 2 0 012-2h2a2 2 0 012 2v2a2 2 0 01-2 2h-2a2 2 0 01-2-2v-2z" /></svg>
              <span className="font-medium">仪表盘</span>
            </NavLink>
            <NavLink to="/cowadmin/keys" onClick={handleNavClick} className={({isActive}) => `flex items-center gap-3 px-4 py-3 rounded-lg transition-colors cursor-pointer ${isActive ? 'bg-teal-50 dark:bg-teal-900/30 text-teal-600 dark:text-teal-400' : 'text-slate-600 dark:text-slate-400 hover:bg-slate-100 dark:hover:bg-slate-700'}`}>
              <svg className="w-5 h-5" fill="none" viewBox="0 0 24 24" stroke="currentColor"><path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M9 5H7a2 2 0 00-2 2v12a2 2 0 002 2h10a2 2 0 002-2V7a2 2 0 00-2-2h-2M9 5a2 2 0 002 2h2a2 2 0 002-2M9 5a2 2 0 012-2h2a2 2 0 012 2" /></svg>
              <span className="font-medium">卡密列表</span>
            </NavLink>
            <NavLink to="/cowadmin/generate" onClick={handleNavClick} className={({isActive}) => `flex items-center gap-3 px-4 py-3 rounded-lg transition-colors cursor-pointer ${isActive ? 'bg-teal-50 dark:bg-teal-900/30 text-teal-600 dark:text-teal-400' : 'text-slate-600 dark:text-slate-400 hover:bg-slate-100 dark:hover:bg-slate-700'}`}>
              <svg className="w-5 h-5" fill="none" viewBox="0 0 24 24" stroke="currentColor"><path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M12 6v6m0 0v6m0-6h6m-6 0H6" /></svg>
              <span className="font-medium">生成卡密</span>
            </NavLink>
            <NavLink to="/cowadmin/products" onClick={handleNavClick} className={({isActive}) => `flex items-center gap-3 px-4 py-3 rounded-lg transition-colors cursor-pointer ${isActive ? 'bg-teal-50 dark:bg-teal-900/30 text-teal-600 dark:text-teal-400' : 'text-slate-600 dark:text-slate-400 hover:bg-slate-100 dark:hover:bg-slate-700'}`}>
              <svg className="w-5 h-5" fill="none" viewBox="0 0 24 24" stroke="currentColor"><path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M19 11H5m14 0a2 2 0 012 2v6a2 2 0 01-2 2H5a2 2 0 01-2-2v-6a2 2 0 012-2m14 0V9a2 2 0 00-2-2M5 11V9a2 2 0 012-2m0 0V5a2 2 0 012-2h6a2 2 0 012 2v2M7 7h10" /></svg>
              <span className="font-medium">套餐管理</span>
            </NavLink>
          </nav>

          <div className="p-3 border-t border-slate-200 dark:border-slate-700">
            <button onClick={onLogout} className="flex items-center gap-3 w-full px-4 py-3 rounded-lg text-red-500 hover:bg-red-50 dark:hover:bg-red-900/20 transition-colors cursor-pointer">
              <svg className="w-5 h-5" fill="none" viewBox="0 0 24 24" stroke="currentColor"><path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M17 16l4-4m0 0l-4-4m4 4H7m6 4v1a3 3 0 01-3 3H6a3 3 0 01-3-3V7a3 3 0 013-3h4a3 3 0 013 3v1" /></svg>
              <span className="font-medium">退出登录</span>
            </button>
          </div>
        </div>
      </aside>

      <main className="lg:ml-64">
        <div className="lg:hidden sticky top-0 z-20 bg-white/80 dark:bg-slate-800/80 backdrop-blur-sm border-b border-slate-200 dark:border-slate-700 px-4 py-3">
          <button onClick={() => setSidebarOpen(true)} className="p-2 rounded-lg hover:bg-slate-100 dark:hover:bg-slate-700 cursor-pointer">
            <svg className="w-6 h-6 text-slate-600 dark:text-slate-400" fill="none" viewBox="0 0 24 24" stroke="currentColor"><path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M4 6h16M4 12h16M4 18h16" /></svg>
          </button>
        </div>
        <div className="p-4 lg:p-6">{children}</div>
      </main>
    </div>
  );
}

function AppRoutes() {
  const [isLoggedIn, setIsLoggedIn] = useState(false);
  const location = useLocation();

  useEffect(() => {
    const secret = localStorage.getItem('admin_secret');
    setIsLoggedIn(!!secret);
  }, []);

  const handleLogout = () => {
    localStorage.removeItem('admin_secret');
    setIsLoggedIn(false);
  };

  // 购买页面是公开的
  if (location.pathname === '/' || location.pathname === '/buy') {
    return <Suspense fallback={<Loading />}><Buy /></Suspense>;
  }

  // 非管理后台路径重定向
  if (!location.pathname.startsWith('/cowadmin')) {
    return <Navigate to="/" replace />;
  }

  // 未登录显示登录页
  if (!isLoggedIn) {
    return <Suspense fallback={<Loading />}><Login onLogin={() => setIsLoggedIn(true)} /></Suspense>;
  }

  return (
    <AdminLayout onLogout={handleLogout}>
      <Suspense fallback={<Loading />}>
        <Routes>
          <Route path="/cowadmin" element={<Dashboard />} />
          <Route path="/cowadmin/keys" element={<Keys />} />
          <Route path="/cowadmin/generate" element={<Generate />} />
          <Route path="/cowadmin/products" element={<Products />} />
          <Route path="*" element={<Navigate to="/cowadmin" />} />
        </Routes>
      </Suspense>
    </AdminLayout>
  );
}

function App() {
  return (
    <BrowserRouter>
      <AppRoutes />
    </BrowserRouter>
  );
}

export default App;
