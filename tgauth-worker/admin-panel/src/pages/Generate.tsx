import { useState } from 'react';
import { generateKeys } from '../api';

export default function Generate() {
  const [count, setCount] = useState(1);
  const [keyType, setKeyType] = useState<'time' | 'permanent'>('time');
  const [durationDays, setDurationDays] = useState(30);
  const [maxDevices, setMaxDevices] = useState(1);
  const [note, setNote] = useState('');
  const [loading, setLoading] = useState(false);
  const [generatedKeys, setGeneratedKeys] = useState<string[]>([]);
  const [error, setError] = useState('');
  const [copied, setCopied] = useState(false);

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault();
    setError('');
    setLoading(true);
    setGeneratedKeys([]);

    try {
      const res = await generateKeys({
        count,
        key_type: keyType,
        duration_days: keyType === 'time' ? durationDays : undefined,
        max_devices: maxDevices,
        created_by: 'admin_panel',
        note: note || undefined
      });

      if (res.data.success) {
        setGeneratedKeys(res.data.data.keys);
      } else {
        setError('生成失败');
      }
    } catch {
      setError('生成失败，请检查网络');
    } finally {
      setLoading(false);
    }
  };

  const copyToClipboard = () => {
    navigator.clipboard.writeText(generatedKeys.join('\n'));
    setCopied(true);
    setTimeout(() => setCopied(false), 2000);
  };

  return (
    <div className="space-y-4 lg:space-y-6">
      {/* Page Header */}
      <div>
        <h1 className="text-xl lg:text-2xl font-bold text-slate-900 dark:text-white">生成卡密</h1>
        <p className="text-sm text-slate-500 dark:text-slate-400 mt-1">批量生成新的授权卡密</p>
      </div>

      <div className="grid grid-cols-1 lg:grid-cols-2 gap-4 lg:gap-6">
        {/* Form Card */}
        <div className="card p-4 lg:p-6">
          <h2 className="text-base lg:text-lg font-semibold text-slate-900 dark:text-white mb-4 lg:mb-6">卡密配置</h2>
          
          <form onSubmit={handleSubmit} className="space-y-4 lg:space-y-5">
            {/* Count */}
            <div>
              <label className="block text-sm font-medium text-slate-700 dark:text-slate-300 mb-2">
                生成数量
              </label>
              <input
                type="number"
                min={1}
                max={100}
                value={count}
                onChange={(e) => setCount(parseInt(e.target.value) || 1)}
                className="input w-full"
              />
              <p className="text-xs text-slate-400 mt-1">最多一次生成 100 个</p>
            </div>

            {/* Key Type */}
            <div>
              <label className="block text-sm font-medium text-slate-700 dark:text-slate-300 mb-2">
                卡密类型
              </label>
              <div className="grid grid-cols-2 gap-2 lg:gap-3">
                <button
                  type="button"
                  onClick={() => setKeyType('time')}
                  className={`p-3 lg:p-4 rounded-xl border-2 transition-all cursor-pointer ${
                    keyType === 'time'
                      ? 'border-teal-500 bg-teal-50 dark:bg-teal-900/20'
                      : 'border-slate-200 dark:border-slate-700 hover:border-slate-300'
                  }`}
                >
                  <div className="flex items-center gap-2 lg:gap-3">
                    <div className={`w-8 h-8 lg:w-10 lg:h-10 rounded-lg flex items-center justify-center ${
                      keyType === 'time' ? 'bg-teal-500 text-white' : 'bg-slate-100 dark:bg-slate-700 text-slate-500'
                    }`}>
                      <svg className="w-4 h-4 lg:w-5 lg:h-5" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                        <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M12 8v4l3 3m6-3a9 9 0 11-18 0 9 9 0 0118 0z" />
                      </svg>
                    </div>
                    <div className="text-left">
                      <p className={`text-sm lg:text-base font-medium ${keyType === 'time' ? 'text-teal-600 dark:text-teal-400' : 'text-slate-700 dark:text-slate-300'}`}>
                        限时
                      </p>
                      <p className="text-xs text-slate-400 hidden sm:block">按天数计费</p>
                    </div>
                  </div>
                </button>
                <button
                  type="button"
                  onClick={() => setKeyType('permanent')}
                  className={`p-3 lg:p-4 rounded-xl border-2 transition-all cursor-pointer ${
                    keyType === 'permanent'
                      ? 'border-teal-500 bg-teal-50 dark:bg-teal-900/20'
                      : 'border-slate-200 dark:border-slate-700 hover:border-slate-300'
                  }`}
                >
                  <div className="flex items-center gap-2 lg:gap-3">
                    <div className={`w-8 h-8 lg:w-10 lg:h-10 rounded-lg flex items-center justify-center ${
                      keyType === 'permanent' ? 'bg-teal-500 text-white' : 'bg-slate-100 dark:bg-slate-700 text-slate-500'
                    }`}>
                      <svg className="w-4 h-4 lg:w-5 lg:h-5" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                        <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M5 3v4M3 5h4M6 17v4m-2-2h4m5-16l2.286 6.857L21 12l-5.714 2.143L13 21l-2.286-6.857L5 12l5.714-2.143L13 3z" />
                      </svg>
                    </div>
                    <div className="text-left">
                      <p className={`text-sm lg:text-base font-medium ${keyType === 'permanent' ? 'text-teal-600 dark:text-teal-400' : 'text-slate-700 dark:text-slate-300'}`}>
                        永久
                      </p>
                      <p className="text-xs text-slate-400 hidden sm:block">永不过期</p>
                    </div>
                  </div>
                </button>
              </div>
            </div>

            {/* Duration Days */}
            {keyType === 'time' && (
              <div>
                <label className="block text-sm font-medium text-slate-700 dark:text-slate-300 mb-2">
                  有效天数
                </label>
                <div className="flex flex-wrap gap-2">
                  {[7, 30, 90, 365].map((d) => (
                    <button
                      key={d}
                      type="button"
                      onClick={() => setDurationDays(d)}
                      className={`px-3 py-1.5 lg:px-4 lg:py-2 rounded-lg text-sm font-medium transition-colors cursor-pointer ${
                        durationDays === d
                          ? 'bg-teal-500 text-white'
                          : 'bg-slate-100 dark:bg-slate-700 text-slate-600 dark:text-slate-300 hover:bg-slate-200 dark:hover:bg-slate-600'
                      }`}
                    >
                      {d}天
                    </button>
                  ))}
                </div>
                <input
                  type="number"
                  min={1}
                  value={durationDays}
                  onChange={(e) => setDurationDays(parseInt(e.target.value) || 30)}
                  className="input w-full mt-2 lg:mt-3"
                  placeholder="自定义天数"
                />
              </div>
            )}

            {/* Max Devices */}
            <div>
              <label className="block text-sm font-medium text-slate-700 dark:text-slate-300 mb-2">
                最大设备数
              </label>
              <input
                type="number"
                min={1}
                max={10}
                value={maxDevices}
                onChange={(e) => setMaxDevices(parseInt(e.target.value) || 1)}
                className="input w-full"
              />
              <p className="text-xs text-slate-400 mt-1">允许同时绑定的设备数量</p>
            </div>

            {/* Note */}
            <div>
              <label className="block text-sm font-medium text-slate-700 dark:text-slate-300 mb-2">
                备注 <span className="text-slate-400 font-normal">(可选)</span>
              </label>
              <input
                type="text"
                value={note}
                onChange={(e) => setNote(e.target.value)}
                className="input w-full"
                placeholder="例如：测试用、VIP客户"
              />
            </div>

            {/* Error */}
            {error && (
              <div className="p-4 rounded-lg bg-red-50 dark:bg-red-900/20 border border-red-200 dark:border-red-800">
                <p className="text-sm text-red-600 dark:text-red-400">{error}</p>
              </div>
            )}

            {/* Submit */}
            <button
              type="submit"
              disabled={loading}
              className="btn btn-primary w-full py-3 flex items-center justify-center gap-2"
            >
              {loading ? (
                <>
                  <svg className="animate-spin w-5 h-5" fill="none" viewBox="0 0 24 24">
                    <circle className="opacity-25" cx="12" cy="12" r="10" stroke="currentColor" strokeWidth="4"></circle>
                    <path className="opacity-75" fill="currentColor" d="M4 12a8 8 0 018-8V0C5.373 0 0 5.373 0 12h4zm2 5.291A7.962 7.962 0 014 12H0c0 3.042 1.135 5.824 3 7.938l3-2.647z"></path>
                  </svg>
                  生成中...
                </>
              ) : (
                <>
                  <svg className="w-5 h-5" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                    <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M12 6v6m0 0v6m0-6h6m-6 0H6" />
                  </svg>
                  生成卡密
                </>
              )}
            </button>
          </form>
        </div>

        {/* Generated Keys Card */}
        <div className="card p-4 lg:p-6">
          <div className="flex items-center justify-between mb-4 lg:mb-6">
            <h2 className="text-base lg:text-lg font-semibold text-slate-900 dark:text-white">生成结果</h2>
            {generatedKeys.length > 0 && (
              <span className="badge bg-emerald-100 text-emerald-700 dark:bg-emerald-900/30 dark:text-emerald-400">
                {generatedKeys.length} 个卡密
              </span>
            )}
          </div>

          {generatedKeys.length === 0 ? (
            <div className="flex flex-col items-center justify-center py-12 lg:py-16 text-slate-400">
              <svg className="w-12 h-12 lg:w-16 lg:h-16 mb-3 lg:mb-4" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1.5} d="M15 7a2 2 0 012 2m4 0a6 6 0 01-7.743 5.743L11 17H9v2H7v2H4a1 1 0 01-1-1v-2.586a1 1 0 01.293-.707l5.964-5.964A6 6 0 1121 9z" />
              </svg>
              <p className="text-sm">生成的卡密将显示在这里</p>
            </div>
          ) : (
            <>
              <div className="space-y-2 max-h-60 lg:max-h-80 overflow-y-auto">
                {generatedKeys.map((key, i) => (
                  <div
                    key={i}
                    className="flex items-center justify-between p-2.5 lg:p-3 rounded-lg bg-slate-50 dark:bg-slate-800/50 hover:bg-slate-100 dark:hover:bg-slate-800 transition-colors"
                  >
                    <code className="text-xs lg:text-sm font-mono text-slate-700 dark:text-slate-300 break-all">{key}</code>
                    <button
                      onClick={() => {
                        navigator.clipboard.writeText(key);
                      }}
                      className="p-1.5 rounded hover:bg-slate-200 dark:hover:bg-slate-700 text-slate-400 hover:text-slate-600 dark:hover:text-slate-300 cursor-pointer"
                      title="复制"
                    >
                      <svg className="w-4 h-4" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                        <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M8 16H6a2 2 0 01-2-2V6a2 2 0 012-2h8a2 2 0 012 2v2m-6 12h8a2 2 0 002-2v-8a2 2 0 00-2-2h-8a2 2 0 00-2 2v8a2 2 0 002 2z" />
                      </svg>
                    </button>
                  </div>
                ))}
              </div>

              <button
                onClick={copyToClipboard}
                className="btn btn-primary w-full mt-4 flex items-center justify-center gap-2"
              >
                {copied ? (
                  <>
                    <svg className="w-5 h-5" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                      <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M5 13l4 4L19 7" />
                    </svg>
                    已复制！
                  </>
                ) : (
                  <>
                    <svg className="w-5 h-5" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                      <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M8 16H6a2 2 0 01-2-2V6a2 2 0 012-2h8a2 2 0 012 2v2m-6 12h8a2 2 0 002-2v-8a2 2 0 00-2-2h-8a2 2 0 00-2 2v8a2 2 0 002 2z" />
                    </svg>
                    复制全部
                  </>
                )}
              </button>
            </>
          )}
        </div>
      </div>
    </div>
  );
}
