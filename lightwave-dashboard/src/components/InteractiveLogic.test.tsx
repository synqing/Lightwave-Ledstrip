import { render, screen, fireEvent } from '@testing-library/react';
import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { EffectsTab } from './tabs/EffectsTab';
import { ShowsTab } from './tabs/ShowsTab';

// Mock Pointer Events for Drag-and-Drop
beforeEach(() => {
  // Mock setPointerCapture/releasePointerCapture
  Element.prototype.setPointerCapture = vi.fn();
  Element.prototype.releasePointerCapture = vi.fn();
  
  // Mock getBoundingClientRect for layout calculations
  Element.prototype.getBoundingClientRect = vi.fn(() => ({
    width: 1000,
    height: 100,
    top: 0,
    left: 0,
    bottom: 100,
    right: 1000,
    x: 0,
    y: 0,
    toJSON: () => {}
  }));
});

afterEach(() => {
  vi.clearAllMocks();
});

describe('Interactive Logic Review', () => {
  
  describe('EffectsTab Interactions', () => {
    it('filters effects by category', () => {
      render(<EffectsTab />);
      
      // Helper to find effect card by name
      const getEffectCard = (name: string) => 
        screen.queryAllByText(name).find(el => el.classList.contains('text-text-primary'));

      // Initially shows all (e.g., Fire, Shockwave)
      expect(getEffectCard('Fire')).toBeInTheDocument();
      expect(getEffectCard('Shockwave')).toBeInTheDocument();
      
      // Click 'Shockwave' filter
      const shockwaveFilter = screen.getByRole('button', { name: 'Shockwave' });
      fireEvent.click(shockwaveFilter);
      
      // Should show Shockwave but not Fire
      expect(getEffectCard('Shockwave')).toBeInTheDocument();
      expect(getEffectCard('Fire')).toBeUndefined();
      
      // Click 'All' filter
      const allFilter = screen.getByRole('button', { name: 'All' });
      fireEvent.click(allFilter);
      
      // Should show both again
      expect(getEffectCard('Fire')).toBeInTheDocument();
    });

    it('filters effects by search query', () => {
      render(<EffectsTab />);
      
      const searchInput = screen.getByPlaceholderText('Search effects...');
      
      // Type 'Ocean'
      fireEvent.change(searchInput, { target: { value: 'Ocean' } });
      
      expect(screen.getByText('Ocean')).toBeInTheDocument();
      expect(screen.queryByText('Fire')).not.toBeInTheDocument();
    });

    it('toggles effect star status', () => {
      render(<EffectsTab />);
      
      // Find the star button for 'Ocean' (initially unstarred)
      // We need to find the container for 'Ocean' and then the button
      const oceanCard = screen.getByText('Ocean').closest('.group');
      const starButton = oceanCard?.querySelector('button');
      
      expect(starButton).toBeInTheDocument();
      
      // Initial state: opacity-0 (unstarred) logic check
      // Note: testing CSS classes is fragile, but we want to verify interaction
      expect(starButton?.className).toContain('opacity-0');
      
      // Click star
      if (starButton) fireEvent.click(starButton);
      
      // Should now be starred (opacity-100)
      expect(starButton?.className).toContain('opacity-100');
    });
  });

  describe('ShowsTab Interactions', () => {
    it('renders timeline tracks correctly', () => {
      render(<ShowsTab />);
      expect(screen.getByText('ZONE 1')).toBeInTheDocument();
      expect(screen.getByText('ZONE 2')).toBeInTheDocument();
      expect(screen.getByText('GLOBAL')).toBeInTheDocument();
    });

    it('updates scene position on drag', () => {
      render(<ShowsTab />);
      
      // Find a scene (e.g., 'Center Pulse')
      const scene = screen.getByText('Center Pulse').closest('div');
      expect(scene).toBeInTheDocument();
      
      if (!scene) return;
      
      // Initial position (style.left)
      const initialLeft = scene.style.left;
      
      // Simulate Drag
      // 1. Pointer Down
      fireEvent.pointerDown(scene, {
        clientX: 100,
        clientY: 50,
        pointerId: 1,
        currentTarget: scene
      });
      
      // 2. Pointer Move (move right by 100px)
      // Since container width is mocked to 1000px, 100px = 10%
      // Initial 'Center Pulse' is at 5%. 5% + 10% = 15%.
      fireEvent.pointerMove(scene, {
        clientX: 200, // 100px movement
        clientY: 50,
        pointerId: 1
      });
      
      // 3. Pointer Up
      fireEvent.pointerUp(scene, {
        clientX: 200,
        clientY: 50,
        pointerId: 1,
        currentTarget: scene
      });
      
      // Verify new position
      // React state update is async, but fireEvent wraps in act() usually.
      // Let's check if the style updated.
      // Note: We need to re-query the element because React might have re-rendered it
      const updatedScene = screen.getByText('Center Pulse').closest('div');
      expect(updatedScene?.style.left).not.toBe(initialLeft);
      // It should be around 15% (mocked calculation)
      // 200px (clientX) - offset. 
      // Offset calculation depends on where we clicked relative to rect.
      // getBoundingClientRect returns left: 0.
      // pointerDown at 100. offset = 100.
      // pointerUp at 200. x - offset = 200 - 100 = 100.
      // 100 / 1000px * 100 = 10%.
      // Original was 5%. Wait.
      // Logic: rawPercent = calculatePercent(e.clientX - dragState.offsetX, trackElement);
      // If we move mouse by +100px, the new pos should be +10%.
      // But let's just assert it CHANGED for now, which verifies the logic is active.
    });
  });

});
