export type ApiMetricKey = `${string} ${string}`;

export interface ApiMetricRow {
  key: ApiMetricKey;
  count: number;
  okCount: number;
  errorCount: number;
  retryCount: number;
  lastStatus?: number;
  lastLatencyMs?: number;
  avgLatencyMs?: number;
  lastAt?: number;
}

export class ApiMetrics {
  private rows = new Map<ApiMetricKey, ApiMetricRow>();

  recordStart(key: ApiMetricKey) {
    const row = this.getOrCreate(key);
    row.count += 1;
    row.lastAt = Date.now();
  }

  recordEnd(key: ApiMetricKey, payload: { ok: boolean; status?: number; latencyMs?: number; retries?: number }) {
    const row = this.getOrCreate(key);
    if (payload.ok) row.okCount += 1;
    else row.errorCount += 1;

    if (typeof payload.retries === 'number') row.retryCount += payload.retries;
    if (typeof payload.status === 'number') row.lastStatus = payload.status;
    if (typeof payload.latencyMs === 'number') {
      row.lastLatencyMs = payload.latencyMs;
      row.avgLatencyMs = row.avgLatencyMs == null ? payload.latencyMs : row.avgLatencyMs * 0.85 + payload.latencyMs * 0.15;
    }
    row.lastAt = Date.now();
  }

  getSnapshot(): ApiMetricRow[] {
    return Array.from(this.rows.values()).sort((a, b) => (b.lastAt ?? 0) - (a.lastAt ?? 0));
  }

  reset() {
    this.rows.clear();
  }

  private getOrCreate(key: ApiMetricKey): ApiMetricRow {
    const existing = this.rows.get(key);
    if (existing) return existing;
    const row: ApiMetricRow = { key, count: 0, okCount: 0, errorCount: 0, retryCount: 0 };
    this.rows.set(key, row);
    return row;
  }
}

