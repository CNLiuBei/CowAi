import { useState, useEffect } from 'react';
import { getKeys, revokeKey, getKeyDetails, unbindDevice } from '../api';
import type { LicenseKey, KeyDevice } from '../api';

export default function Keys() {
  const [keys, setKeys] = useState<LicenseKey[]>([]);
  const [total, setTotal] = useState(0);
  const [loading, setLoading] = useState(true);
  const [exporting, setExporting] = useState(false);
  const [status, setStatus] = useState('');
  const [page, setPage] = useState(0);
  const [showExportMenu, setShowExportMenu] = useState(false);
  const [copiedKey, setCopiedKey] = useState<string | null>(null);
  const [searchKey, setSearchKey] = useState('');
  const [selectedKey, setSelectedKey] = useState<LicenseKey | null>(null);
  const [keyDevices, setKeyDevices] = useState<KeyDevice[]>([]);
  const [showDetailModal, setShowDetailModal] = useState(false);
  const [detailLoading, setDetailLoading] = useState(false);
  const limit = 10;

  useEffect(() => {
    loadKeys();
  }, [status, page]);

  const loadKeys = async () => {
    setLoading(true);
    try {
      const res = await getKeys({ status: status || undefined, limit, offset: page * limit });
      if (res.data.success) {
        setKeys(res.data.data.keys);
        setTotal(res.data.data.total);
      }
    } catch (err) {
      console.error('Failed to load keys:', err);
    } finally {
      setLoading(false);
    }
  };

  const handleRevoke = async (keyCode: string) => {
    if (!confirm(`确定要撤销卡密 ${keyCode} 吗？`)) return;
    try {
      await revokeKey(keyCode);
      loadKeys();
      if (selectedKey?.key_code === keyCode) {
        setShowDetailModal(false);
      }
    } catch {
      alert('撤销失败');
    }
  };

  const handleViewDetails = async (key: LicenseKey) => {
    setSelectedKey(key);
    setShowDetailModal(true);
    setDetailLoading(true);
    try {
      const res = await getKeyDetails(key.key_code);
      if (res.data.success) {
        setKeyDevices(res.data.data.devices);
      }
    } catch (err) {
      console.error('Failed to load key details:', err);
      alert('加载详情失败');
    } finally {
      setDetailLoading(false);
    }
  };

  const handleUnbindDevice = async (deviceId: string) => {
    if (!selectedKey) return;
    if (!confirm(`确定要解绑设备 ${deviceId} 吗？`)) return;
    try {
      await unbindDevice(selectedKey.key_code, deviceId);
      // 重新加载设备列表
      const res = await getKeyDetails(selectedKey.key_code);
      if (res.data.success) {
        setKeyDevices(res.data.data.devices);
      }
    } catch {
      alert('解绑失败');
    }
  };

  const handleSearch = async () => {
    if (!searchKey.trim()) {
      loadKeys();
      return;
    }
    setLoading(true);
    try {
      const res = await getKeyDetails(searchKey.trim());
      if (res.data.success) {
        setKeys([res.data.data.key]);
        setTotal(1);
      } else {
        setKeys([]);
        setTotal(0);
      }
    } catch {
      setKeys([]);
      setTotal(0);
    } finally {
      setLoading(false);
    }
  };

  const handleClearSearch = () => {
    setSearchKey('');
    loadKeys();
  };

  const copyKey = (keyCode: string) => {
    navigator.clipboard.writeText(keyCode).then(() => {
      setCopiedKey(keyCode);
      setTimeout(() => setCopiedKey(null), 1500);
    });
  };

  const formatDate = (ts: number | null) => {
    if (!ts) return '-';
    return new Date(ts * 1000).toLocaleString('zh-CN');
  };

  const formatDateShort = (ts: number | null) => {
    if (!ts) return '-';
    return new Date(ts * 1000).toLocaleDateString('zh-CN');
  };

  const formatDateForExport = (ts: number | null) => {
    if (!ts) return '';
    return new Date(ts * 1000).toISOString().replace('T', ' ').substring(0, 19);
  };

  const getStatusLabel = (s: string) => {
    const labels: Record<string, string> = {
      unused: '未使用',
      activated: '已激活',
      expired: '已过期',
      revoked: '已撤销',
    };
    return labels[s] || s;
  };

  const getStatusBadge = (s: string) => {
    const styles: Record<string, string> = {
      unused: 'bg-blue-100 text-blue-700 dark:bg-blue-900/30 dark:text-blue-400',
      activated: 'bg-emerald-100 text-emerald-700 dark:bg-emerald-900/30 dark:text-emerald-400',
      expired: 'bg-amber-100 text-amber-700 dark:bg-amber-900/30 dark:text-amber-400',
      revoked: 'bg-red-100 text-red-700 dark:bg-red-900/30 dark:text-red-400',
    };
    return <span className={`badge ${styles[s] || ''}`}>{getStatusLabel(s)}</span>;
  };

  const exportKeys = async (format: 'csv' | 'txt' | 'txt-keys-only') => {
    setExporting(true);
    setShowExportMenu(false);
    
    try {
      const res = await getKeys({ status: status || undefined, limit: 10000, offset: 0 });
      if (!res.data.success) {
        alert('导出失败');
        return;
      }
      
      const allKeys = res.data.data.keys;
      let content = '';
      let filename = '';
      let mimeType = '';
      
      const timestamp = new Date().toISOString().slice(0, 10);
      const statusSuffix = status ? `_${status}` : '_all';
      
      if (format === 'csv') {
        const headers = ['卡密', '类型', '状态', '有效天数', '最大设备数', '创建时间', '到期时间', '备注'];
        const rows = allKeys.map(k => [
          k.key_code,
          k.key_type === 'permanent' ? '永久' : '限时',
          getStatusLabel(k.status),
          k.duration_days?.toString() || '',
          k.max_devices.toString(),
          formatDateForExport(k.created_at),
          formatDateForExport(k.expires_at),
          k.note || ''
        ]);
        content = '\uFEFF' + headers.join(',') + '\n' + rows.map(r => r.map(c => `"${c}"`).join(',')).join('\n');
        filename = `keys${statusSuffix}_${timestamp}.csv`;
        mimeType = 'text/csv;charset=utf-8';
      } else if (format === 'txt') {
        content = allKeys.map(k => 
          `${k.key_code} | ${k.key_type === 'permanent' ? '永久' : k.duration_days + '天'} | ${getStatusLabel(k.status)}`
        ).join('\n');
        filename = `keys${statusSuffix}_${timestamp}.txt`;
        mimeType = 'text/plain;charset=utf-8';
      } else {
        content = allKeys.map(k => k.key_code).join('\n');
        filename = `keys${statusSuffix}_${timestamp}_codes.txt`;
        mimeType = 'text/plain;charset=utf-8';
      }
      
      const blob = new Blob([content], { type: mimeType });
      const url = URL.createObjectURL(blob);
      const a = document.createElement('a');
      a.href = url;
      a.download = filename;
      document.body.appendChild(a);
      a.click();
      document.body.removeChild(a);
      URL.revokeObjectURL(url);
    } catch (err) {
      console.error('Export failed:', err);
      alert('导出失败');
    } finally {
      setExporting(false);
    }
  };

  const totalPages = Math.ceil(total / limit);

  return (
    <div className="space-y-4 lg:space-y-6">
      {/* Page Header */}
      <div className="flex flex-col gap-4">
        <div>
          <h1 className="text-xl lg:text-2xl font-bold text-slate-900 dark:text-white">卡密列表</h1>
          <p className="text-sm text-slate-500 dark:text-slate-400 mt-1">共 {total} 条记录</p>
        </div>
        
        {/* Filters & Actions */}
        <div className="flex flex-wrap items-center gap-2">
          {/* Search Input */}
          <div className="flex items-center gap-2 flex-1 min-w-[200px] max-w-md">
            <input
              type="text"
              value={searchKey}
              onChange={(e) => setSearchKey(e.target.value)}
              onKeyDown={(e) => e.key === 'Enter' && handleSearch()}
              placeholder="搜索卡密..."
              className="input flex-1 text-sm"
            />
            {searchKey && (
              <button onClick={handleClearSearch} className="btn btn-ghost p-2" title="清除搜索">
                <svg className="w-5 h-5" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                  <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M6 18L18 6M6 6l12 12" />
                </svg>
              </button>
            )}
            <button onClick={handleSearch} className="btn btn-primary text-sm px-4 py-2">
              搜索
            </button>
          </div>
          
          <select
            value={status}
            onChange={(e) => { setStatus(e.target.value); setPage(0); setSearchKey(''); }}
            className="input flex-1 min-w-[120px] max-w-[160px] text-sm"
          >
            <option value="">全部状态</option>
            <option value="unused">未使用</option>
            <option value="activated">已激活</option>
            <option value="expired">已过期</option>
            <option value="revoked">已撤销</option>
          </select>
          
          {/* Export Button */}
          <div className="relative">
            <button 
              onClick={() => setShowExportMenu(!showExportMenu)}
              disabled={exporting || total === 0}
              className="btn btn-primary flex items-center gap-1.5 text-sm px-3 py-2 disabled:opacity-50"
            >
              {exporting ? (
                <svg className="animate-spin w-4 h-4" fill="none" viewBox="0 0 24 24">
                  <circle className="opacity-25" cx="12" cy="12" r="10" stroke="currentColor" strokeWidth="4"></circle>
                  <path className="opacity-75" fill="currentColor" d="M4 12a8 8 0 018-8V0C5.373 0 0 5.373 0 12h4zm2 5.291A7.962 7.962 0 014 12H0c0 3.042 1.135 5.824 3 7.938l3-2.647z"></path>
                </svg>
              ) : (
                <svg className="w-4 h-4" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                  <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M4 16v1a3 3 0 003 3h10a3 3 0 003-3v-1m-4-4l-4 4m0 0l-4-4m4 4V4" />
                </svg>
              )}
              <span className="hidden sm:inline">导出</span>
              <svg className="w-3 h-3" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M19 9l-7 7-7-7" />
              </svg>
            </button>
            
            {showExportMenu && (
              <>
                <div className="fixed inset-0 z-10" onClick={() => setShowExportMenu(false)}></div>
                <div className="absolute right-0 mt-2 w-44 bg-white dark:bg-slate-800 rounded-lg shadow-lg border border-slate-200 dark:border-slate-700 py-1 z-20">
                  <button onClick={() => exportKeys('csv')} className="w-full px-3 py-2 text-left text-sm text-slate-700 dark:text-slate-300 hover:bg-slate-100 dark:hover:bg-slate-700 flex items-center gap-2">
                    <svg className="w-4 h-4 text-emerald-500" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                      <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M9 17v-2m3 2v-4m3 4v-6m2 10H7a2 2 0 01-2-2V5a2 2 0 012-2h5.586a1 1 0 01.707.293l5.414 5.414a1 1 0 01.293.707V19a2 2 0 01-2 2z" />
                    </svg>
                    CSV (完整)
                  </button>
                  <button onClick={() => exportKeys('txt')} className="w-full px-3 py-2 text-left text-sm text-slate-700 dark:text-slate-300 hover:bg-slate-100 dark:hover:bg-slate-700 flex items-center gap-2">
                    <svg className="w-4 h-4 text-blue-500" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                      <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M9 12h6m-6 4h6m2 5H7a2 2 0 01-2-2V5a2 2 0 012-2h5.586a1 1 0 01.707.293l5.414 5.414a1 1 0 01.293.707V19a2 2 0 01-2 2z" />
                    </svg>
                    TXT (带状态)
                  </button>
                  <button onClick={() => exportKeys('txt-keys-only')} className="w-full px-3 py-2 text-left text-sm text-slate-700 dark:text-slate-300 hover:bg-slate-100 dark:hover:bg-slate-700 flex items-center gap-2">
                    <svg className="w-4 h-4 text-violet-500" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                      <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M15 7a2 2 0 012 2m4 0a6 6 0 01-7.743 5.743L11 17H9v2H7v2H4a1 1 0 01-1-1v-2.586a1 1 0 01.293-.707l5.964-5.964A6 6 0 1121 9z" />
                    </svg>
                    TXT (仅卡密)
                  </button>
                </div>
              </>
            )}
          </div>
          
          <button onClick={loadKeys} className="btn btn-ghost p-2">
            <svg className="w-5 h-5" fill="none" viewBox="0 0 24 24" stroke="currentColor">
              <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M4 4v5h.582m15.356 2A8.001 8.001 0 004.582 9m0 0H9m11 11v-5h-.581m0 0a8.003 8.003 0 01-15.357-2m15.357 2H15" />
            </svg>
          </button>
        </div>
      </div>

      {/* Desktop Table */}
      <div className="hidden lg:block card overflow-hidden">
        <div className="overflow-x-auto">
          <table className="w-full">
            <thead>
              <tr className="bg-slate-50 dark:bg-slate-800/50">
                <th className="px-6 py-4 text-left text-xs font-semibold text-slate-500 dark:text-slate-400 uppercase tracking-wider">卡密</th>
                <th className="px-6 py-4 text-left text-xs font-semibold text-slate-500 dark:text-slate-400 uppercase tracking-wider">类型</th>
                <th className="px-6 py-4 text-left text-xs font-semibold text-slate-500 dark:text-slate-400 uppercase tracking-wider">状态</th>
                <th className="px-6 py-4 text-left text-xs font-semibold text-slate-500 dark:text-slate-400 uppercase tracking-wider">设备数</th>
                <th className="px-6 py-4 text-left text-xs font-semibold text-slate-500 dark:text-slate-400 uppercase tracking-wider">创建时间</th>
                <th className="px-6 py-4 text-left text-xs font-semibold text-slate-500 dark:text-slate-400 uppercase tracking-wider">到期时间</th>
                <th className="px-6 py-4 text-center text-xs font-semibold text-slate-500 dark:text-slate-400 uppercase tracking-wider">操作</th>
              </tr>
            </thead>
            <tbody className="divide-y divide-slate-200 dark:divide-slate-700">
              {loading ? (
                <tr>
                  <td colSpan={7} className="px-6 py-12 text-center text-slate-500">
                    <svg className="animate-spin w-8 h-8 mx-auto text-teal-500" fill="none" viewBox="0 0 24 24">
                      <circle className="opacity-25" cx="12" cy="12" r="10" stroke="currentColor" strokeWidth="4"></circle>
                      <path className="opacity-75" fill="currentColor" d="M4 12a8 8 0 018-8V0C5.373 0 0 5.373 0 12h4zm2 5.291A7.962 7.962 0 014 12H0c0 3.042 1.135 5.824 3 7.938l3-2.647z"></path>
                    </svg>
                  </td>
                </tr>
              ) : keys.length === 0 ? (
                <tr>
                  <td colSpan={7} className="px-6 py-12 text-center text-slate-500">暂无数据</td>
                </tr>
              ) : keys.map((key) => (
                <tr key={key.id} className="hover:bg-slate-50 dark:hover:bg-slate-800/50 transition-colors">
                  <td className="px-6 py-4">
                    <div className="flex items-center gap-2">
                      <code className="text-sm font-mono bg-slate-100 dark:bg-slate-700 px-2 py-1 rounded">{key.key_code}</code>
                      <button 
                        onClick={() => copyKey(key.key_code)}
                        className="p-1 rounded hover:bg-slate-200 dark:hover:bg-slate-600 text-slate-400 hover:text-slate-600 dark:hover:text-slate-300 transition-colors"
                        title="复制"
                      >
                        {copiedKey === key.key_code ? (
                          <svg className="w-4 h-4 text-emerald-500" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                            <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M5 13l4 4L19 7" />
                          </svg>
                        ) : (
                          <svg className="w-4 h-4" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                            <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M8 16H6a2 2 0 01-2-2V6a2 2 0 012-2h8a2 2 0 012 2v2m-6 12h8a2 2 0 002-2v-8a2 2 0 00-2-2h-8a2 2 0 00-2 2v8a2 2 0 002 2z" />
                          </svg>
                        )}
                      </button>
                    </div>
                  </td>
                  <td className="px-6 py-4 text-sm text-slate-600 dark:text-slate-300">
                    {key.key_type === 'permanent' ? '永久' : `${key.duration_days}天`}
                  </td>
                  <td className="px-6 py-4">{getStatusBadge(key.status)}</td>
                  <td className="px-6 py-4 text-sm text-slate-600 dark:text-slate-300">{key.max_devices}</td>
                  <td className="px-6 py-4 text-sm text-slate-500 dark:text-slate-400">{formatDate(key.created_at)}</td>
                  <td className="px-6 py-4 text-sm text-slate-500 dark:text-slate-400">{formatDate(key.expires_at)}</td>
                  <td className="px-6 py-4">
                    <div className="flex items-center justify-center gap-3">
                      <button 
                        onClick={() => handleViewDetails(key)} 
                        className="text-teal-600 hover:text-teal-700 dark:text-teal-400 dark:hover:text-teal-300 text-sm font-medium"
                      >
                        详情
                      </button>
                      {key.status !== 'revoked' && (
                        <button 
                          onClick={() => handleRevoke(key.key_code)} 
                          className="text-red-500 hover:text-red-700 text-sm font-medium"
                        >
                          撤销
                        </button>
                      )}
                    </div>
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>

        {/* Desktop Pagination */}
        {totalPages > 1 && (
          <div className="flex items-center justify-between px-6 py-4 border-t border-slate-200 dark:border-slate-700">
            <p className="text-sm text-slate-500">
              显示 {page * limit + 1} - {Math.min((page + 1) * limit, total)} 条，共 {total} 条
            </p>
            <div className="flex items-center gap-2">
              <button onClick={() => setPage(p => p - 1)} disabled={page === 0} className="btn btn-ghost text-sm disabled:opacity-50">
                上一页
              </button>
              <span className="text-sm text-slate-600 dark:text-slate-400">{page + 1} / {totalPages}</span>
              <button onClick={() => setPage(p => p + 1)} disabled={page >= totalPages - 1} className="btn btn-ghost text-sm disabled:opacity-50">
                下一页
              </button>
            </div>
          </div>
        )}
      </div>

      {/* Mobile Card List */}
      <div className="lg:hidden space-y-3">
        {loading ? (
          <div className="card p-8 text-center">
            <svg className="animate-spin w-8 h-8 mx-auto text-teal-500" fill="none" viewBox="0 0 24 24">
              <circle className="opacity-25" cx="12" cy="12" r="10" stroke="currentColor" strokeWidth="4"></circle>
              <path className="opacity-75" fill="currentColor" d="M4 12a8 8 0 018-8V0C5.373 0 0 5.373 0 12h4zm2 5.291A7.962 7.962 0 014 12H0c0 3.042 1.135 5.824 3 7.938l3-2.647z"></path>
            </svg>
          </div>
        ) : keys.length === 0 ? (
          <div className="card p-8 text-center text-slate-500">暂无数据</div>
        ) : keys.map((key) => (
          <div key={key.id} className="card p-4">
            {/* Key Code & Status */}
            <div className="flex items-start justify-between gap-2 mb-3">
              <div className="flex-1 min-w-0">
                <div className="flex items-center gap-2 mb-1">
                  {getStatusBadge(key.status)}
                  <span className="text-xs text-slate-400">
                    {key.key_type === 'permanent' ? '永久' : `${key.duration_days}天`}
                  </span>
                </div>
                <code className="text-sm font-mono text-slate-700 dark:text-slate-300 break-all">{key.key_code}</code>
              </div>
              <button 
                onClick={() => copyKey(key.key_code)}
                className="p-2 rounded-lg hover:bg-slate-100 dark:hover:bg-slate-700 text-slate-400 transition-colors"
                title="复制"
              >
                {copiedKey === key.key_code ? (
                  <svg className="w-4 h-4 text-emerald-500" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                    <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M5 13l4 4L19 7" />
                  </svg>
                ) : (
                  <svg className="w-4 h-4" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                    <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M8 16H6a2 2 0 01-2-2V6a2 2 0 012-2h8a2 2 0 012 2v2m-6 12h8a2 2 0 002-2v-8a2 2 0 00-2-2h-8a2 2 0 00-2 2v8a2 2 0 002 2z" />
                  </svg>
                )}
              </button>
            </div>
            
            {/* Info Grid */}
            <div className="grid grid-cols-2 gap-2 text-xs mb-3">
              <div>
                <span className="text-slate-400">设备数：</span>
                <span className="text-slate-600 dark:text-slate-300">{key.max_devices}</span>
              </div>
              <div>
                <span className="text-slate-400">创建：</span>
                <span className="text-slate-600 dark:text-slate-300">{formatDateShort(key.created_at)}</span>
              </div>
              {key.expires_at && (
                <div className="col-span-2">
                  <span className="text-slate-400">到期：</span>
                  <span className="text-slate-600 dark:text-slate-300">{formatDate(key.expires_at)}</span>
                </div>
              )}
            </div>
            
            {/* Actions */}
            <div className="pt-3 border-t border-slate-100 dark:border-slate-700 flex items-center gap-3">
              <button 
                onClick={() => handleViewDetails(key)}
                className="text-teal-600 dark:text-teal-400 text-sm font-medium"
              >
                查看详情
              </button>
              {key.status !== 'revoked' && (
                <button 
                  onClick={() => handleRevoke(key.key_code)}
                  className="text-red-500 text-sm font-medium"
                >
                  撤销卡密
                </button>
              )}
            </div>
          </div>
        ))}

        {/* Mobile Pagination */}
        {totalPages > 1 && (
          <div className="flex items-center justify-between py-2">
            <button 
              onClick={() => setPage(p => p - 1)} 
              disabled={page === 0} 
              className="btn btn-ghost text-sm disabled:opacity-50"
            >
              上一页
            </button>
            <span className="text-sm text-slate-500">{page + 1} / {totalPages}</span>
            <button 
              onClick={() => setPage(p => p + 1)} 
              disabled={page >= totalPages - 1} 
              className="btn btn-ghost text-sm disabled:opacity-50"
            >
              下一页
            </button>
          </div>
        )}
      </div>

      {/* Key Detail Modal */}
      {showDetailModal && selectedKey && (
        <div className="fixed inset-0 z-50 flex items-center justify-center p-4 bg-black/50" onClick={() => setShowDetailModal(false)}>
          <div className="bg-white dark:bg-slate-800 rounded-xl shadow-2xl max-w-3xl w-full max-h-[90vh] overflow-hidden" onClick={(e) => e.stopPropagation()}>
            {/* Modal Header */}
            <div className="flex items-center justify-between px-6 py-4 border-b border-slate-200 dark:border-slate-700">
              <h2 className="text-xl font-bold text-slate-900 dark:text-white">卡密详情</h2>
              <button 
                onClick={() => setShowDetailModal(false)}
                className="p-2 rounded-lg hover:bg-slate-100 dark:hover:bg-slate-700 text-slate-400 hover:text-slate-600 dark:hover:text-slate-300 transition-colors"
              >
                <svg className="w-5 h-5" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                  <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M6 18L18 6M6 6l12 12" />
                </svg>
              </button>
            </div>

            {/* Modal Body */}
            <div className="p-6 overflow-y-auto max-h-[calc(90vh-140px)]">
              {/* Key Info */}
              <div className="mb-6">
                <h3 className="text-sm font-semibold text-slate-500 dark:text-slate-400 uppercase tracking-wider mb-3">卡密信息</h3>
                <div className="bg-slate-50 dark:bg-slate-900/50 rounded-lg p-4 space-y-3">
                  <div className="flex items-start justify-between">
                    <div className="flex-1">
                      <p className="text-xs text-slate-500 dark:text-slate-400 mb-1">卡密代码</p>
                      <code className="text-sm font-mono text-slate-900 dark:text-white">{selectedKey.key_code}</code>
                    </div>
                    <button 
                      onClick={() => copyKey(selectedKey.key_code)}
                      className="p-2 rounded-lg hover:bg-slate-200 dark:hover:bg-slate-700 text-slate-400 transition-colors"
                    >
                      {copiedKey === selectedKey.key_code ? (
                        <svg className="w-4 h-4 text-emerald-500" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                          <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M5 13l4 4L19 7" />
                        </svg>
                      ) : (
                        <svg className="w-4 h-4" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                          <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M8 16H6a2 2 0 01-2-2V6a2 2 0 012-2h8a2 2 0 012 2v2m-6 12h8a2 2 0 002-2v-8a2 2 0 00-2-2h-8a2 2 0 00-2 2v8a2 2 0 002 2z" />
                        </svg>
                      )}
                    </button>
                  </div>
                  
                  <div className="grid grid-cols-2 gap-4">
                    <div>
                      <p className="text-xs text-slate-500 dark:text-slate-400 mb-1">状态</p>
                      {getStatusBadge(selectedKey.status)}
                    </div>
                    <div>
                      <p className="text-xs text-slate-500 dark:text-slate-400 mb-1">类型</p>
                      <p className="text-sm text-slate-900 dark:text-white">
                        {selectedKey.key_type === 'permanent' ? '永久' : `${selectedKey.duration_days}天`}
                      </p>
                    </div>
                    <div>
                      <p className="text-xs text-slate-500 dark:text-slate-400 mb-1">最大设备数</p>
                      <p className="text-sm text-slate-900 dark:text-white">{selectedKey.max_devices}</p>
                    </div>
                    <div>
                      <p className="text-xs text-slate-500 dark:text-slate-400 mb-1">已绑定设备</p>
                      <p className="text-sm text-slate-900 dark:text-white">
                        {keyDevices.filter(d => d.is_active).length}
                      </p>
                    </div>
                    <div>
                      <p className="text-xs text-slate-500 dark:text-slate-400 mb-1">创建时间</p>
                      <p className="text-sm text-slate-900 dark:text-white">{formatDate(selectedKey.created_at)}</p>
                    </div>
                    {selectedKey.activated_at && (
                      <div>
                        <p className="text-xs text-slate-500 dark:text-slate-400 mb-1">激活时间</p>
                        <p className="text-sm text-slate-900 dark:text-white">{formatDate(selectedKey.activated_at)}</p>
                      </div>
                    )}
                    {selectedKey.expires_at && (
                      <div>
                        <p className="text-xs text-slate-500 dark:text-slate-400 mb-1">到期时间</p>
                        <p className="text-sm text-slate-900 dark:text-white">{formatDate(selectedKey.expires_at)}</p>
                      </div>
                    )}
                    {selectedKey.note && (
                      <div className="col-span-2">
                        <p className="text-xs text-slate-500 dark:text-slate-400 mb-1">备注</p>
                        <p className="text-sm text-slate-900 dark:text-white">{selectedKey.note}</p>
                      </div>
                    )}
                  </div>
                </div>
              </div>

              {/* Devices List */}
              <div>
                <h3 className="text-sm font-semibold text-slate-500 dark:text-slate-400 uppercase tracking-wider mb-3">
                  绑定设备 ({keyDevices.filter(d => d.is_active).length})
                </h3>
                
                {detailLoading ? (
                  <div className="text-center py-8">
                    <svg className="animate-spin w-8 h-8 mx-auto text-teal-500" fill="none" viewBox="0 0 24 24">
                      <circle className="opacity-25" cx="12" cy="12" r="10" stroke="currentColor" strokeWidth="4"></circle>
                      <path className="opacity-75" fill="currentColor" d="M4 12a8 8 0 018-8V0C5.373 0 0 5.373 0 12h4zm2 5.291A7.962 7.962 0 014 12H0c0 3.042 1.135 5.824 3 7.938l3-2.647z"></path>
                    </svg>
                  </div>
                ) : keyDevices.length === 0 ? (
                  <div className="text-center py-8 text-slate-500 dark:text-slate-400">
                    暂无绑定设备
                  </div>
                ) : (
                  <div className="space-y-2">
                    {keyDevices.map((device) => (
                      <div 
                        key={device.device_id} 
                        className={`bg-slate-50 dark:bg-slate-900/50 rounded-lg p-4 ${!device.is_active ? 'opacity-50' : ''}`}
                      >
                        <div className="flex items-start justify-between gap-4">
                          <div className="flex-1 min-w-0">
                            <div className="flex items-center gap-2 mb-2">
                              <code className="text-sm font-mono text-slate-900 dark:text-white break-all">
                                {device.device_id}
                              </code>
                              {!device.is_active && (
                                <span className="badge bg-slate-200 text-slate-600 dark:bg-slate-700 dark:text-slate-400 text-xs">
                                  已解绑
                                </span>
                              )}
                            </div>
                            <div className="grid grid-cols-2 gap-2 text-xs">
                              <div>
                                <span className="text-slate-400">绑定时间：</span>
                                <span className="text-slate-600 dark:text-slate-300">{formatDate(device.bound_at)}</span>
                              </div>
                              <div>
                                <span className="text-slate-400">最后活跃：</span>
                                <span className="text-slate-600 dark:text-slate-300">{formatDate(device.last_seen)}</span>
                              </div>
                            </div>
                          </div>
                          {device.is_active && (
                            <button 
                              onClick={() => handleUnbindDevice(device.device_id)}
                              className="text-red-500 hover:text-red-700 text-sm font-medium whitespace-nowrap"
                            >
                              解绑
                            </button>
                          )}
                        </div>
                      </div>
                    ))}
                  </div>
                )}
              </div>
            </div>

            {/* Modal Footer */}
            <div className="flex items-center justify-end gap-3 px-6 py-4 border-t border-slate-200 dark:border-slate-700">
              {selectedKey.status !== 'revoked' && (
                <button 
                  onClick={() => handleRevoke(selectedKey.key_code)}
                  className="btn bg-red-500 hover:bg-red-600 text-white"
                >
                  撤销卡密
                </button>
              )}
              <button 
                onClick={() => setShowDetailModal(false)}
                className="btn btn-ghost"
              >
                关闭
              </button>
            </div>
          </div>
        </div>
      )}
    </div>
  );
}
