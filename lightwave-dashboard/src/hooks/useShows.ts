import { useCallback, useEffect, useMemo, useState } from 'react';
import { V2Client } from '../services/v2/client';
import { buildV2BaseUrl } from '../services/v2/config';
import { useV2 } from '../state/v2';
import type { Show } from '../types/timeline';

export interface ShowState {
  showId: number | string | null;
  showName: string | null;
  playbackId?: number;
  isPlaying: boolean;
  isPaused: boolean;
  progress: number;
  elapsedMs: number;
  remainingMs: number;
  currentChapter: number | null;
}

interface ShowListResponse {
  builtin: Array<{
    id: number;
    name: string;
    durationMs: number;
    durationSeconds: number;
    playbackId?: number;
    chapterCount?: number;
    cueCount?: number;
    looping: boolean;
    type: 'builtin';
  }>;
  custom: Array<{
    id: string;
    name: string;
    durationMs: number;
    durationSeconds: number;
    playbackId?: number;
    type: 'custom';
    isSaved: boolean;
  }>;
}

export function useShows() {
  const { state, actions } = useV2();
  const client = useMemo(() => new V2Client({
    baseUrl: buildV2BaseUrl(state.settings.deviceOrigin),
    auth: { mode: state.settings.authMode, token: state.settings.authToken || undefined },
    requestTimeoutMs: 5000,
  }), [state.settings.authMode, state.settings.authToken, state.settings.deviceOrigin]);

  const [shows, setShows] = useState<Show[]>([]);
  const [currentShow, setCurrentShow] = useState<ShowState | null>(null);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);

  // Load shows from firmware
  const loadShows = useCallback(async () => {
    setLoading(true);
    setError(null);
    try {
      const data = await client.get<ShowListResponse>('/shows');
      const allShows: Show[] = [
        ...data.builtin.map((s) => ({
          id: String(s.id),
          name: s.name,
          durationSeconds: s.durationSeconds,
          scenes: [],
          isSaved: true,
        })),
        ...data.custom.map((s) => ({
          id: s.id,
          name: s.name,
          durationSeconds: s.durationSeconds,
          scenes: [],
          isSaved: s.isSaved,
        })),
      ];
      setShows(allShows);
    } catch (err) {
      const msg = err instanceof Error ? err.message : 'Failed to load shows';
      setError(msg);
    } finally {
      setLoading(false);
    }
  }, [client]);

  // Load a specific show with scenes
  const loadShow = useCallback(async (showId: string | number): Promise<Show | null> => {
    setLoading(true);
    setError(null);
    try {
      const data = await client.get<Show>(`/shows/${encodeURIComponent(String(showId))}`, { query: { format: 'scenes' } });
      return data;
    } catch (err) {
      const msg = err instanceof Error ? err.message : 'Failed to load show';
      setError(msg);
      return null;
    } finally {
      setLoading(false);
    }
  }, [client]);

  // Save show to firmware
  const saveShow = useCallback(async (show: Show): Promise<string | null> => {
    setLoading(true);
    setError(null);
    try {
      const body = {
        name: show.name,
        durationSeconds: show.durationSeconds,
        scenes: show.scenes.map(s => ({
          id: s.id,
          zoneId: s.zoneId,
          effectName: s.effectName,
          startTimePercent: s.startTimePercent,
          durationPercent: s.durationPercent,
        })),
      };

      // Built-ins are numeric IDs; only custom shows can be updated.
      const isBuiltinId = /^[0-9]+$/.test(show.id);
      if (!isBuiltinId && show.id && !show.id.startsWith('new-')) {
        await client.put<{ id: string; name: string; message: string; playbackId?: number }>(`/shows/${encodeURIComponent(show.id)}`, body);
        return show.id;
      }

      const created = await client.post<{ id: string; name: string; message: string; playbackId?: number }>('/shows', body);
      return created.id;
    } catch (err) {
      const msg = err instanceof Error ? err.message : 'Failed to save show';
      setError(msg);
      return null;
    } finally {
      setLoading(false);
    }
  }, [client]);

  // Delete show
  const deleteShow = useCallback(async (showId: string): Promise<boolean> => {
    setLoading(true);
    setError(null);
    try {
      await client.delete<{ id: string; message: string }>(`/shows/${encodeURIComponent(showId)}`);
      return true;
    } catch (err) {
      const msg = err instanceof Error ? err.message : 'Failed to delete show';
      setError(msg);
      return false;
    } finally {
      setLoading(false);
    }
  }, [client]);

  // Control show playback
  const controlShow = useCallback(async (action: 'start' | 'stop' | 'pause' | 'resume' | 'seek', showId?: number | string, timeMs?: number): Promise<boolean> => {
    setError(null);
    try {
      const payload: Record<string, unknown> = { action };
      if (action === 'start' && showId !== undefined) {
        payload.showId = showId;
      }
      if (action === 'seek' && timeMs !== undefined) {
        payload.timeMs = timeMs;
      }
      await client.post('/shows/control', payload);
      return true;
    } catch (err) {
      const msg = err instanceof Error ? err.message : 'Failed to control show';
      setError(msg);
      return false;
    }
  }, [client]);

  // Get current show state
  const getCurrentState = useCallback(async () => {
    try {
      const data = await client.get<ShowState>('/shows/current');
      setCurrentShow(data);
    } catch {
      // Ignore errors for polling
    }
  }, [client]);

  // WebSocket event handlers (best-effort; show events may not be broadcast on all builds)
  useEffect(() => {
    const ws = actions.getWsClient();
    if (!ws) return;

    const off = ws.onEvent((ev) => {
      const anyEv = ev as any;
      if (anyEv.type === 'show.state' && anyEv.success === true && anyEv.data) {
        setCurrentShow(anyEv.data as ShowState);
      }
      if (anyEv.type === 'show.stopped') {
        setCurrentShow(null);
      }
    });

    // Poll for current state every 100ms during playback
    const interval = setInterval(() => {
      if (currentShow?.isPlaying) {
        getCurrentState();
      }
    }, 100);

    return () => {
      off();
      clearInterval(interval);
    };
  }, [actions, currentShow, getCurrentState]);

  return {
    shows,
    currentShow,
    loading,
    error,
    loadShows,
    loadShow,
    saveShow,
    deleteShow,
    controlShow,
    getCurrentState,
  };
}
