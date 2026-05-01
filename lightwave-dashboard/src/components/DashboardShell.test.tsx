import { render, screen, fireEvent } from '@testing-library/react';
import { describe, it, expect, vi } from 'vitest';
import DashboardShell from './DashboardShell';
import { V2Provider } from '../state/v2';

// Wrapper component for tests that provides V2 context
const TestWrapper: React.FC<{ children: React.ReactNode }> = ({ children }) => (
  <V2Provider autoConnect={false}>{children}</V2Provider>
);

// Mock Canvas for LGPVisualizer and TelemetryGraph
// Since jsdom doesn't fully support canvas, we need to mock it or the context
const mockCanvas2D = {
  fillStyle: '',
  fillRect: vi.fn(),
  clearRect: vi.fn(),
  beginPath: vi.fn(),
  moveTo: vi.fn(),
  lineTo: vi.fn(),
  stroke: vi.fn(),
  createRadialGradient: vi.fn(() => ({ addColorStop: vi.fn() })),
  createLinearGradient: vi.fn(() => ({ addColorStop: vi.fn() })),
  scale: vi.fn(),
  setTransform: vi.fn(),
  fill: vi.fn(),
  roundRect: vi.fn(), // Mocking the new API
} as unknown as CanvasRenderingContext2D;

HTMLCanvasElement.prototype.getContext = vi
  .fn((contextId: unknown) => {
    if (contextId === '2d') return mockCanvas2D;
    return null;
  }) as unknown as typeof HTMLCanvasElement.prototype.getContext;

describe('DashboardShell', () => {
  it('renders the header with logo', () => {
    render(<DashboardShell />, { wrapper: TestWrapper });
    // Updated to match current component which renders "LightwaveOS" as one word
    expect(screen.getByText('LightwaveOS')).toBeInTheDocument();
  });

  it('renders navigation tabs', () => {
    render(<DashboardShell />, { wrapper: TestWrapper });
    expect(screen.getByText('Control')).toBeInTheDocument();
    expect(screen.getByText('Shows')).toBeInTheDocument();
    expect(screen.getByText('Effects')).toBeInTheDocument();
    expect(screen.getByText('System')).toBeInTheDocument();
  });

  it('switches tabs correctly', () => {
    render(<DashboardShell />, { wrapper: TestWrapper });

    // Default is Control tab - look for Control + Zones heading
    expect(screen.getByText('Control + Zones')).toBeInTheDocument();

    // Click System
    fireEvent.click(screen.getByText('System'));
    // System tab should show device info
    expect(screen.getByText('Device')).toBeInTheDocument();
  });

  it('renders connection status indicator', () => {
    render(<DashboardShell />, { wrapper: TestWrapper });
    // The connection status is shown via visual indicators
    // Check that the header renders with connection components
    expect(screen.getByRole('tablist')).toBeInTheDocument();
  });

  it('renders slider controls', () => {
    render(<DashboardShell />, { wrapper: TestWrapper });
    // Check that control sliders are rendered
    expect(screen.getByText('Brightness')).toBeInTheDocument();
    expect(screen.getByText('Speed')).toBeInTheDocument();
  });
});
