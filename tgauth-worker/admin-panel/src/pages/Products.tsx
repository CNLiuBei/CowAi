import { useState, useEffect } from 'react';
import { getProducts, createProduct, updateProduct, deleteProduct } from '../api';
import type { Product } from '../api';

export default function Products() {
  const [products, setProducts] = useState<Product[]>([]);
  const [loading, setLoading] = useState(true);
  const [showModal, setShowModal] = useState(false);
  const [editingProduct, setEditingProduct] = useState<Product | null>(null);
  const [saving, setSaving] = useState(false);

  // 表单状态
  const [form, setForm] = useState({
    id: '',
    name: '',
    description: '',
    price: '',
    key_type: 'time' as 'time' | 'permanent',
    duration_days: '30',
    max_devices: '1',
    sort_order: '0',
  });

  useEffect(() => {
    loadProducts();
  }, []);

  const loadProducts = async () => {
    setLoading(true);
    try {
      const res = await getProducts();
      if (res.data.success) {
        setProducts(res.data.data.products);
      }
    } catch (err) {
      console.error('Failed to load products:', err);
    } finally {
      setLoading(false);
    }
  };

  const openCreateModal = () => {
    setEditingProduct(null);
    setForm({
      id: '',
      name: '',
      description: '',
      price: '',
      key_type: 'time',
      duration_days: '30',
      max_devices: '1',
      sort_order: '0',
    });
    setShowModal(true);
  };

  const openEditModal = (product: Product) => {
    setEditingProduct(product);
    setForm({
      id: product.id,
      name: product.name,
      description: product.description || '',
      price: product.price.toString(),
      key_type: product.key_type,
      duration_days: product.duration_days?.toString() || '30',
      max_devices: product.max_devices.toString(),
      sort_order: product.sort_order.toString(),
    });
    setShowModal(true);
  };

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault();
    setSaving(true);

    try {
      const data = {
        id: form.id,
        name: form.name,
        description: form.description || null,
        price: parseFloat(form.price),
        key_type: form.key_type,
        duration_days: form.key_type === 'time' ? parseInt(form.duration_days) : null,
        max_devices: parseInt(form.max_devices),
        sort_order: parseInt(form.sort_order),
      };

      if (editingProduct) {
        await updateProduct(editingProduct.id, data);
      } else {
        await createProduct(data);
      }

      setShowModal(false);
      loadProducts();
    } catch (err) {
      console.error('Failed to save product:', err);
      alert('保存失败');
    } finally {
      setSaving(false);
    }
  };

  const handleDelete = async (product: Product) => {
    if (!confirm(`确定要删除套餐「${product.name}」吗？`)) return;

    try {
      await deleteProduct(product.id);
      loadProducts();
    } catch (err) {
      console.error('Failed to delete product:', err);
      alert('删除失败');
    }
  };

  const handleToggleActive = async (product: Product) => {
    try {
      await updateProduct(product.id, { is_active: product.is_active ? 0 : 1 });
      loadProducts();
    } catch (err) {
      console.error('Failed to toggle product:', err);
    }
  };

  return (
    <div className="space-y-4 lg:space-y-6">
      {/* Header */}
      <div className="flex flex-col sm:flex-row sm:items-center sm:justify-between gap-4">
        <div>
          <h1 className="text-xl lg:text-2xl font-bold text-slate-900 dark:text-white">套餐管理</h1>
          <p className="text-sm text-slate-500 dark:text-slate-400 mt-1">管理销售套餐和价格</p>
        </div>
        <button onClick={openCreateModal} className="btn btn-primary flex items-center gap-2">
          <svg className="w-5 h-5" fill="none" viewBox="0 0 24 24" stroke="currentColor">
            <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M12 6v6m0 0v6m0-6h6m-6 0H6" />
          </svg>
          添加套餐
        </button>
      </div>

      {/* Products List */}
      <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-4">
        {loading ? (
          <div className="col-span-full text-center py-12">
            <svg className="animate-spin w-8 h-8 mx-auto text-teal-500" fill="none" viewBox="0 0 24 24">
              <circle className="opacity-25" cx="12" cy="12" r="10" stroke="currentColor" strokeWidth="4"></circle>
              <path className="opacity-75" fill="currentColor" d="M4 12a8 8 0 018-8V0C5.373 0 0 5.373 0 12h4zm2 5.291A7.962 7.962 0 014 12H0c0 3.042 1.135 5.824 3 7.938l3-2.647z"></path>
            </svg>
          </div>
        ) : products.length === 0 ? (
          <div className="col-span-full card p-8 text-center text-slate-500">
            暂无套餐，点击上方按钮添加
          </div>
        ) : products.map((product) => (
          <div key={product.id} className={`card p-4 ${!product.is_active ? 'opacity-60' : ''}`}>
            <div className="flex items-start justify-between mb-3">
              <div>
                <div className="flex items-center gap-2">
                  <h3 className="font-semibold text-slate-900 dark:text-white">{product.name}</h3>
                  {!product.is_active && (
                    <span className="badge bg-slate-100 text-slate-500 dark:bg-slate-700 dark:text-slate-400">已下架</span>
                  )}
                </div>
                <p className="text-sm text-slate-500 mt-1">{product.description || '无描述'}</p>
              </div>
              <span className="text-xl font-bold text-teal-600">¥{product.price}</span>
            </div>

            <div className="space-y-1 text-sm text-slate-600 dark:text-slate-400 mb-4">
              <div className="flex justify-between">
                <span>类型</span>
                <span>{product.key_type === 'permanent' ? '永久' : '限时'}</span>
              </div>
              {product.key_type === 'time' && (
                <div className="flex justify-between">
                  <span>有效期</span>
                  <span>{product.duration_days} 天</span>
                </div>
              )}
              <div className="flex justify-between">
                <span>设备数</span>
                <span>{product.max_devices}</span>
              </div>
              <div className="flex justify-between">
                <span>排序</span>
                <span>{product.sort_order}</span>
              </div>
            </div>

            <div className="flex items-center gap-2 pt-3 border-t border-slate-200 dark:border-slate-700">
              <button
                onClick={() => handleToggleActive(product)}
                className={`flex-1 py-1.5 rounded-lg text-sm font-medium ${
                  product.is_active
                    ? 'bg-amber-100 text-amber-700 dark:bg-amber-900/30 dark:text-amber-400'
                    : 'bg-emerald-100 text-emerald-700 dark:bg-emerald-900/30 dark:text-emerald-400'
                }`}
              >
                {product.is_active ? '下架' : '上架'}
              </button>
              <button
                onClick={() => openEditModal(product)}
                className="flex-1 py-1.5 rounded-lg text-sm font-medium bg-slate-100 text-slate-700 dark:bg-slate-700 dark:text-slate-300"
              >
                编辑
              </button>
              <button
                onClick={() => handleDelete(product)}
                className="p-1.5 rounded-lg text-red-500 hover:bg-red-50 dark:hover:bg-red-900/20"
              >
                <svg className="w-5 h-5" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                  <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M19 7l-.867 12.142A2 2 0 0116.138 21H7.862a2 2 0 01-1.995-1.858L5 7m5 4v6m4-6v6m1-10V4a1 1 0 00-1-1h-4a1 1 0 00-1 1v3M4 7h16" />
                </svg>
              </button>
            </div>
          </div>
        ))}
      </div>

      {/* Modal */}
      {showModal && (
        <div className="fixed inset-0 z-50 flex items-center justify-center p-4">
          <div className="fixed inset-0 bg-black/50" onClick={() => setShowModal(false)}></div>
          <div className="relative bg-white dark:bg-slate-800 rounded-2xl shadow-xl w-full max-w-md max-h-[90vh] overflow-y-auto">
            <div className="p-6">
              <h2 className="text-lg font-semibold text-slate-900 dark:text-white mb-4">
                {editingProduct ? '编辑套餐' : '添加套餐'}
              </h2>

              <form onSubmit={handleSubmit} className="space-y-4">
                {!editingProduct && (
                  <div>
                    <label className="block text-sm font-medium text-slate-700 dark:text-slate-300 mb-1">套餐ID</label>
                    <input
                      type="text"
                      value={form.id}
                      onChange={(e) => setForm({ ...form, id: e.target.value })}
                      className="input w-full"
                      placeholder="如: week, month"
                      required
                    />
                    <p className="text-xs text-slate-400 mt-1">唯一标识，创建后不可修改</p>
                  </div>
                )}

                <div>
                  <label className="block text-sm font-medium text-slate-700 dark:text-slate-300 mb-1">套餐名称</label>
                  <input
                    type="text"
                    value={form.name}
                    onChange={(e) => setForm({ ...form, name: e.target.value })}
                    className="input w-full"
                    placeholder="如: 月卡"
                    required
                  />
                </div>

                <div>
                  <label className="block text-sm font-medium text-slate-700 dark:text-slate-300 mb-1">描述</label>
                  <input
                    type="text"
                    value={form.description}
                    onChange={(e) => setForm({ ...form, description: e.target.value })}
                    className="input w-full"
                    placeholder="如: 30天有效期"
                  />
                </div>

                <div>
                  <label className="block text-sm font-medium text-slate-700 dark:text-slate-300 mb-1">价格 (元)</label>
                  <input
                    type="number"
                    step="0.01"
                    min="0.01"
                    value={form.price}
                    onChange={(e) => setForm({ ...form, price: e.target.value })}
                    className="input w-full"
                    required
                  />
                </div>

                <div>
                  <label className="block text-sm font-medium text-slate-700 dark:text-slate-300 mb-1">卡密类型</label>
                  <select
                    value={form.key_type}
                    onChange={(e) => setForm({ ...form, key_type: e.target.value as 'time' | 'permanent' })}
                    className="input w-full"
                  >
                    <option value="time">限时卡</option>
                    <option value="permanent">永久卡</option>
                  </select>
                </div>

                {form.key_type === 'time' && (
                  <div>
                    <label className="block text-sm font-medium text-slate-700 dark:text-slate-300 mb-1">有效天数</label>
                    <input
                      type="number"
                      min="1"
                      value={form.duration_days}
                      onChange={(e) => setForm({ ...form, duration_days: e.target.value })}
                      className="input w-full"
                      required
                    />
                  </div>
                )}

                <div>
                  <label className="block text-sm font-medium text-slate-700 dark:text-slate-300 mb-1">最大设备数</label>
                  <input
                    type="number"
                    min="1"
                    max="10"
                    value={form.max_devices}
                    onChange={(e) => setForm({ ...form, max_devices: e.target.value })}
                    className="input w-full"
                    required
                  />
                </div>

                <div>
                  <label className="block text-sm font-medium text-slate-700 dark:text-slate-300 mb-1">排序 (越小越靠前)</label>
                  <input
                    type="number"
                    min="0"
                    value={form.sort_order}
                    onChange={(e) => setForm({ ...form, sort_order: e.target.value })}
                    className="input w-full"
                  />
                </div>

                <div className="flex gap-3 pt-4">
                  <button
                    type="button"
                    onClick={() => setShowModal(false)}
                    className="flex-1 btn btn-ghost"
                  >
                    取消
                  </button>
                  <button
                    type="submit"
                    disabled={saving}
                    className="flex-1 btn btn-primary"
                  >
                    {saving ? '保存中...' : '保存'}
                  </button>
                </div>
              </form>
            </div>
          </div>
        </div>
      )}
    </div>
  );
}
