import { describe, expect, it, vi } from 'vitest';
import { v2Api } from './api';
import type { V2Client } from './client';

describe('v2Api', () => {
  it('uses page-first list request semantics', async () => {
    const get = vi.fn(async () => ({}));
    const api = v2Api({ get } as unknown as V2Client);

    await api.effectsList({ page: 2, limit: 25 });
    await api.palettesList({ page: 3, limit: 10 });

    expect(get).toHaveBeenNthCalledWith(1, '/effects', { query: { page: 2, limit: 25 } });
    expect(get).toHaveBeenNthCalledWith(2, '/palettes', {
      query: {
        page: 3,
        limit: 10,
        category: undefined,
        warm: undefined,
        cool: undefined,
        calm: undefined,
        vivid: undefined,
        cvd: undefined,
      },
    });
  });
});
