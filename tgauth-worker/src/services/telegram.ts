import { Env } from '../types';

interface SendMessageOptions {
  parse_mode?: 'Markdown' | 'HTML';
  reply_markup?: {
    inline_keyboard?: Array<Array<{
      text: string;
      callback_data?: string;
      url?: string;
    }>>;
  };
}

export class TelegramService {
  private env: Env;
  private baseUrl: string;

  constructor(env: Env) {
    this.env = env;
    this.baseUrl = `https://api.telegram.org/bot${env.BOT_TOKEN}`;
  }

  // 发送消息
  async sendMessage(
    chatId: number,
    text: string,
    options?: SendMessageOptions
  ): Promise<boolean> {
    try {
      const response = await fetch(`${this.baseUrl}/sendMessage`, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify({
          chat_id: chatId,
          text: text,
          parse_mode: options?.parse_mode,
          reply_markup: options?.reply_markup,
        }),
      });

      const result = await response.json() as { ok: boolean };
      return result.ok;
    } catch (error) {
      console.error('Error sending message:', error);
      return false;
    }
  }

  // 编辑消息
  async editMessage(
    chatId: number,
    messageId: number,
    text: string,
    options?: SendMessageOptions
  ): Promise<boolean> {
    try {
      const response = await fetch(`${this.baseUrl}/editMessageText`, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify({
          chat_id: chatId,
          message_id: messageId,
          text: text,
          parse_mode: options?.parse_mode,
          reply_markup: options?.reply_markup,
        }),
      });

      const result = await response.json() as { ok: boolean };
      return result.ok;
    } catch (error) {
      console.error('Error editing message:', error);
      return false;
    }
  }

  // 回答回调查询
  async answerCallbackQuery(
    callbackQueryId: string,
    text?: string,
    showAlert?: boolean
  ): Promise<boolean> {
    try {
      const response = await fetch(`${this.baseUrl}/answerCallbackQuery`, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify({
          callback_query_id: callbackQueryId,
          text: text,
          show_alert: showAlert,
        }),
      });

      const result = await response.json() as { ok: boolean };
      return result.ok;
    } catch (error) {
      console.error('Error answering callback query:', error);
      return false;
    }
  }

  // 设置 Webhook
  async setWebhook(url: string, secretToken?: string): Promise<boolean> {
    try {
      const response = await fetch(`${this.baseUrl}/setWebhook`, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify({
          url: url,
          secret_token: secretToken,
          allowed_updates: ['message', 'callback_query'],
        }),
      });

      const result = await response.json() as { ok: boolean };
      return result.ok;
    } catch (error) {
      console.error('Error setting webhook:', error);
      return false;
    }
  }

  // 删除 Webhook
  async deleteWebhook(): Promise<boolean> {
    try {
      const response = await fetch(`${this.baseUrl}/deleteWebhook`, {
        method: 'POST',
      });

      const result = await response.json() as { ok: boolean };
      return result.ok;
    } catch (error) {
      console.error('Error deleting webhook:', error);
      return false;
    }
  }

  // 获取 Webhook 信息
  async getWebhookInfo(): Promise<unknown> {
    try {
      const response = await fetch(`${this.baseUrl}/getWebhookInfo`);
      return await response.json();
    } catch (error) {
      console.error('Error getting webhook info:', error);
      return null;
    }
  }
}
