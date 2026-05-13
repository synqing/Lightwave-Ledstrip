import React, { useEffect } from 'react';
import { render, waitFor } from '@testing-library/react';
import { afterEach, describe, expect, it, vi } from 'vitest';
import { V2Provider, useV2 } from './v2';

const RefetchProbe: React.FC = () => {
  const { actions } = useV2();

  useEffect(() => {
    actions.setParameters({ brightness: 42 }, { debounceMs: 0 });
  }, [actions]);

  return null;
};

describe('v2 mutation refetch', () => {
  afterEach(() => {
    vi.unstubAllGlobals();
    localStorage.clear();
  });

  it('refetches canonical parameters after an acknowledgement-only mutation', async () => {
    const fetchMock = vi
      .fn()
      .mockResolvedValueOnce(new Response(JSON.stringify({
        success: true,
        timestamp: 1,
        version: '1.0.0',
      })))
      .mockResolvedValueOnce(new Response(JSON.stringify({
        success: true,
        data: {
          brightness: 42,
          speed: 20,
          hue: 0,
          paletteId: 1,
          intensity: 80,
          saturation: 200,
          complexity: 50,
          variation: 0,
        },
        timestamp: 2,
        version: '1.0.0',
      })));

    vi.stubGlobal('fetch', fetchMock);

    render(
      <V2Provider autoConnect={false}>
        <RefetchProbe />
      </V2Provider>
    );

    await waitFor(() => expect(fetchMock).toHaveBeenCalledTimes(2));
    expect(fetchMock.mock.calls[0][0]).toContain('/parameters');
    expect(fetchMock.mock.calls[0][1]).toMatchObject({ method: 'PATCH' });
    expect(fetchMock.mock.calls[1][0]).toContain('/parameters');
    expect(fetchMock.mock.calls[1][1]).toMatchObject({ method: 'GET' });
  });
});
