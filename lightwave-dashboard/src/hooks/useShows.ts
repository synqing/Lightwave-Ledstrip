import { useState, useEffect, useCallback } from 'react';
import { useV2 } from '../state/v2';
import type { Show, TimelineScene } from '../types/timeline';

export interface ShowState {
  showId: number | string | null;
  showName: string | null;
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
    type: 'custom';
    isSaved: boolean;
  }>;
}

export function useShows() {
  const { ws, http } = useV2();
  const [shows, setShows] = useState<Show[]>([]);
  const [currentShow, setCurrentShow] = useState<ShowState | null>(null);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);

  // Load shows from firmware
  const loadShows = useCallback(async () => {
    setLoading(true);
    setError(null);
    try {
      const response = await http.get<{ success: boolean; data: ShowListResponse }>('/api/v1/shows');
      if (response.data.success) {
        const allShows: Show[] = [
          ...response.data.data.builtin.map(s => ({
            id: String(s.id),
            name: s.name,
            durationSeconds: s.durationSeconds,
            scenes: [], // Will be loaded on demand
            isSaved: true,
          })),
          ...response.data.data.custom.map(s => ({
            id: s.id,
            name: s.name,
            durationSeconds: s.durationSeconds,
            scenes: [], // Will be loaded on demand
            isSaved: s.isSaved,
          })),
        ];
        setShows(allShows);
      }
    } catch (err: any) {
      setError(err.message || 'Failed to load shows');
    } finally {
      setLoading(false);
    }
  }, [http]);

  // Load a specific show with scenes
  const loadShow = useCallback(async (showId: string | number): Promise<Show | null> => {
    setLoading(true);
    setError(null);
    try {
      const response = await http.get<{ success: boolean; data: Show }>(`/api/v1/shows/${showId}?format=scenes`);
      if (response.data.success) {
        return response.data.data;
      }
      return null;
    } catch (err: any) {
      setError(err.message || 'Failed to load show');
      return null;
    } finally {
      setLoading(false);
    }
  }, [http]);

  // Save show to firmware
  const saveShow = useCallback(async (show: Show): Promise<string | null> => {
    setLoading(true);
    setError(null);
    try {
      const response = await http.post<{ success: boolean; data: { id: string; name: string; message: string } }>('/api/v1/shows', {
        name: show.name,
        durationSeconds: show.durationSeconds,
        scenes: show.scenes.map(s => ({
          id: s.id,
          zoneId: s.zoneId,
          effectName: s.effectName,
          startTimePercent: s.startTimePercent,
          durationPercent: s.durationPercent,
        })),
      });
      if (response.data.success) {
        return response.data.data.id;
      }
      return null;
    } catch (err: any) {
      setError(err.message || 'Failed to save show');
      return null;
    } finally {
      setLoading(false);
    }
  }, [http]);

  // Delete show
  const deleteShow = useCallback(async (showId: string): Promise<boolean> => {
    setLoading(true);
    setError(null);
    try {
      const response = await http.delete<{ success: boolean; data: { id: string; message: string } }>(`/api/v1/shows/${showId}`);
      return response.data.success;
    } catch (err: any) {
      setError(err.message || 'Failed to delete show');
      return false;
    } finally {
      setLoading(false);
    }
  }, [http]);

  // Control show playback
  const controlShow = useCallback(async (action: 'start' | 'stop' | 'pause' | 'resume' | 'seek', showId?: number, timeMs?: number): Promise<boolean> => {
    setError(null);
    try {
      const payload: any = { action };
      if (action === 'start' && showId !== undefined) {
        payload.showId = showId;
      }
      if (action === 'seek' && timeMs !== undefined) {
        payload.timeMs = timeMs;
      }
      const response = await http.post<{ success: boolean; data: { action: string; message: string } }>('/api/v1/shows/control', payload);
      return response.data.success;
    } catch (err: any) {
      setError(err.message || 'Failed to control show');
      return false;
    }
  }, [http]);

  // Get current show state
  const getCurrentState = useCallback(async () => {
    try {
      const response = await http.get<{ success: boolean; data: ShowState }>('/api/v1/shows/current');
      if (response.data.success) {
        setCurrentShow(response.data.data);
      }
    } catch (err: any) {
      // Ignore errors for state polling
    }
  }, [http]);

  // WebSocket event handlers
  useEffect(() => {
    if (!ws) return;

    const handlers: Record<string, (data: any) => void> = {
      'show.started': (data: any) => {
        setCurrentShow(prev => prev ? { ...prev, isPlaying: true, isPaused: false } : {
          showId: data.showId,
          showName: data.showName,
          isPlaying: true,
          isPaused: false,
          progress: 0,
          elapsedMs: 0,
          remainingMs: 0,
          currentChapter: null,
        });
      },
      'show.stopped': (data: any) => {
        setCurrentShow(null);
      },
      'show.paused': (data: any) => {
        setCurrentShow(prev => prev ? { ...prev, isPaused: true } : null);
      },
      'show.resumed': (data: any) => {
        setCurrentShow(prev => prev ? { ...prev, isPaused: false } : null);
      },
      'show.progress': (data: any) => {
        setCurrentShow(prev => prev ? { ...prev, progress: data.progress, elapsedMs: data.elapsedMs, remainingMs: data.remainingMs } : null);
      },
      'show.chapterChanged': (data: any) => {
        setCurrentShow(prev => prev ? { ...prev, currentChapter: data.chapterIndex } : null);
      },
    };

    Object.entries(handlers).forEach(([type, handler]) => {
      ws.on(type, handler);
    });

    // Poll for current state every 100ms during playback
    const interval = setInterval(() => {
      if (currentShow?.isPlaying) {
        getCurrentState();
      }
    }, 100);

    return () => {
      Object.keys(handlers).forEach(type => {
        ws.off(type);
      });
      clearInterval(interval);
    };
  }, [ws, currentShow, getCurrentState]);

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

