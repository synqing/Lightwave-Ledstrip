import { ApiMetrics } from './metrics';
import type { V2Envelope, V2ErrorBody } from './types';
import { V2ApiError } from './types';

export type V2AuthMode = 'none' | 'bearer' | 'apiKey';

export interface V2ClientOptions {
  baseUrl: string;
  auth?: { mode: V2AuthMode; token?: string };
  metrics?: ApiMetrics;
  requestTimeoutMs?: number;
}

type HttpMethod = 'GET' | 'POST' | 'PUT' | 'PATCH' | 'DELETE';

interface CacheEntry<T> {
  expiresAt: number;
  value: Promise<T>;
}

export class V2Client {
  private baseUrl: string;
  private auth: { mode: V2AuthMode; token?: string };
  private metrics?: ApiMetrics;
  private requestTimeoutMs: number;
  private cache = new Map<string, CacheEntry<unknown>>();

  constructor(opts: V2ClientOptions) {
    this.baseUrl = opts.baseUrl.replace(/\/+$/, '');
    this.auth = opts.auth ?? { mode: 'none' };
    this.metrics = opts.metrics;
    this.requestTimeoutMs = opts.requestTimeoutMs ?? 5000;
  }

  setBaseUrl(baseUrl: string) {
    this.baseUrl = baseUrl.replace(/\/+$/, '');
    this.cache.clear();
  }

  setAuth(auth: { mode: V2AuthMode; token?: string }) {
    this.auth = auth;
  }

  async get<T>(path: string, opts?: { query?: Record<string, string | number | boolean | undefined> }) {
    return this.request<T>('GET', path, { query: opts?.query });
  }

  async getCached<T>(path: string, opts: { ttlMs: number; query?: Record<string, string | number | boolean | undefined> }) {
    const url = this.buildUrl(path, opts.query);
    const key = `GET ${url}`;
    const now = Date.now();
    const existing = this.cache.get(key) as CacheEntry<T> | undefined;
    if (existing && existing.expiresAt > now) return existing.value;
    const value = this.request<T>('GET', path, { query: opts.query, urlOverride: url });
    this.cache.set(key, { expiresAt: now + opts.ttlMs, value });
    return value;
  }

  async patch<T>(path: string, body: unknown) {
    this.invalidateGetCache();
    return this.request<T>('PATCH', path, { body });
  }

  async put<T>(path: string, body: unknown) {
    this.invalidateGetCache();
    return this.request<T>('PUT', path, { body });
  }

  async post<T>(path: string, body: unknown) {
    this.invalidateGetCache();
    return this.request<T>('POST', path, { body });
  }

  private invalidateGetCache() {
    for (const key of this.cache.keys()) {
      if (key.startsWith('GET ')) this.cache.delete(key);
    }
  }

  private buildUrl(path: string, query?: Record<string, string | number | boolean | undefined>) {
    const normalizedPath = path.startsWith('/') ? path : `/${path}`;
    const url = new URL(`${this.baseUrl}${normalizedPath}`, window.location.origin);
    if (query) {
      for (const [k, v] of Object.entries(query)) {
        if (v === undefined) continue;
        url.searchParams.set(k, String(v));
      }
    }
    return url.toString().replace(window.location.origin, '');
  }

  private async request<T>(
    method: HttpMethod,
    path: string,
    opts?: { query?: Record<string, string | number | boolean | undefined>; body?: unknown; urlOverride?: string }
  ): Promise<T> {
    const url = opts?.urlOverride ?? this.buildUrl(path, opts?.query);
    const key = `${method} ${url}` as const;
    const start = performance.now();
    this.metrics?.recordStart(key);

    const headers: Record<string, string> = {
      Accept: 'application/json',
    };
    if (method !== 'GET') headers['Content-Type'] = 'application/json';

    if (this.auth.mode === 'bearer' && this.auth.token) headers.Authorization = `Bearer ${this.auth.token}`;
    if (this.auth.mode === 'apiKey' && this.auth.token) headers['X-API-Key'] = this.auth.token;

    const retryableStatus = new Set([408, 429, 500, 502, 503, 504]);
    const maxAttempts = 4;
    let attempt = 0;
    let lastStatus: number | undefined;

    while (attempt < maxAttempts) {
      attempt += 1;
      const controller = new AbortController();
      const timeoutId = window.setTimeout(() => controller.abort(), this.requestTimeoutMs);

      try {
        const res = await fetch(url, {
          method,
          headers,
          body: opts?.body == null ? undefined : JSON.stringify(opts.body),
          signal: controller.signal,
        });
        window.clearTimeout(timeoutId);
        lastStatus = res.status;

        if (!res.ok) {
          const retryAfter = res.headers.get('Retry-After');
          if (retryableStatus.has(res.status) && attempt < maxAttempts) {
            await this.delay(this.computeBackoffMs(attempt, retryAfter));
            continue;
          }
        }

        const parsed = await this.safeParseJson(res);
        if (!parsed) {
          const msg = `Invalid JSON response`;
          this.metrics?.recordEnd(key, {
            ok: false,
            status: res.status,
            latencyMs: performance.now() - start,
            retries: attempt - 1,
          });
          throw new V2ApiError(msg, { httpStatus: res.status, url });
        }

        const envelope = parsed as V2Envelope<T>;
        if ('success' in envelope && envelope.success === false) {
          const errBody = envelope.error as V2ErrorBody | undefined;
          this.metrics?.recordEnd(key, {
            ok: false,
            status: res.status,
            latencyMs: performance.now() - start,
            retries: attempt - 1,
          });
          throw new V2ApiError(errBody?.message ?? 'API error', { httpStatus: res.status, body: errBody, url });
        }

        if ('success' in envelope && envelope.success === true) {
          this.metrics?.recordEnd(key, {
            ok: true,
            status: res.status,
            latencyMs: performance.now() - start,
            retries: attempt - 1,
          });
          return envelope.data as T;
        }

        this.metrics?.recordEnd(key, {
          ok: true,
          status: res.status,
          latencyMs: performance.now() - start,
          retries: attempt - 1,
        });
        return parsed as T;
      } catch (err) {
        window.clearTimeout(timeoutId);
        const isAbort = err instanceof DOMException && err.name === 'AbortError';
        const isNetwork = err instanceof TypeError;

        if ((isAbort || isNetwork) && attempt < maxAttempts) {
          await this.delay(this.computeBackoffMs(attempt));
          continue;
        }

        this.metrics?.recordEnd(key, {
          ok: false,
          status: lastStatus,
          latencyMs: performance.now() - start,
          retries: attempt - 1,
        });
        throw err;
      }
    }

    this.metrics?.recordEnd(key, {
      ok: false,
      status: lastStatus,
      latencyMs: performance.now() - start,
      retries: attempt - 1,
    });
    throw new V2ApiError('Request failed after retries', { httpStatus: lastStatus, url });
  }

  private async safeParseJson(res: Response) {
    const text = await res.text();
    if (!text) return null;
    try {
      return JSON.parse(text);
    } catch {
      return null;
    }
  }

  private computeBackoffMs(attempt: number, retryAfter?: string | null) {
    if (retryAfter) {
      const seconds = Number(retryAfter);
      if (!Number.isNaN(seconds) && seconds > 0) return Math.min(seconds * 1000, 5000);
    }
    const base = 150 * Math.pow(2, attempt - 1);
    const jitter = Math.random() * 100;
    return Math.min(base + jitter, 2500);
  }

  private delay(ms: number) {
    return new Promise<void>(resolve => window.setTimeout(resolve, ms));
  }
}

