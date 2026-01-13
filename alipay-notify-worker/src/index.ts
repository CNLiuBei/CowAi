/**
 * 支付宝回调中转 Worker
 * 根据订单号前缀分发到不同的目标系统
 */

interface Env {
  // 路由配置，格式：ROUTE_前缀 = 目标URL
  [key: string]: string;
}

// 从环境变量中获取路由映射
function getRoutes(env: Env): Map<string, string> {
  const routes = new Map<string, string>();
  for (const [key, value] of Object.entries(env)) {
    if (key.startsWith('ROUTE_') && typeof value === 'string') {
      const prefix = key.replace('ROUTE_', '');
      routes.set(prefix, value);
    }
  }
  return routes;
}

// 根据订单号匹配目标URL
function matchRoute(orderNo: string, routes: Map<string, string>): string | null {
  for (const [prefix, url] of routes) {
    if (orderNo.startsWith(prefix)) {
      return url;
    }
  }
  return null;
}

export default {
  async fetch(request: Request, env: Env): Promise<Response> {
    // 只处理 POST 请求
    if (request.method !== 'POST') {
      return new Response('Method Not Allowed', { status: 405 });
    }

    try {
      // 解析表单数据
      const formData = await request.formData();
      const params: Record<string, string> = {};
      formData.forEach((value, key) => {
        params[key] = value.toString();
      });

      const orderNo = params.out_trade_no;
      if (!orderNo) {
        console.error('Missing out_trade_no');
        return new Response('fail');
      }

      console.log(`Received notify for order: ${orderNo}`);

      // 获取路由配置
      const routes = getRoutes(env);
      const targetUrl = matchRoute(orderNo, routes);

      if (!targetUrl) {
        console.error(`No route found for order: ${orderNo}`);
        // 返回 success 避免支付宝重试
        return new Response('success');
      }

      console.log(`Forwarding to: ${targetUrl}`);

      // 转发请求到目标系统
      const forwardResponse = await fetch(targetUrl, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/x-www-form-urlencoded',
        },
        body: new URLSearchParams(params).toString(),
      });

      const responseText = await forwardResponse.text();
      console.log(`Target response: ${responseText}`);

      // 返回目标系统的响应
      return new Response(responseText);
    } catch (error) {
      console.error('Error processing notify:', error);
      return new Response('fail');
    }
  },
};
