import React, { useState, useRef, useEffect, useId } from 'react';
import { ChevronDown } from 'lucide-react';

/**
 * Accessible Listbox Component
 * 
 * A fully keyboard-accessible dropdown/listbox component that follows
 * WAI-ARIA patterns for combobox/listbox interactions.
 * 
 * @component
 * 
 * @example
 * ```tsx
 * <Listbox
 *   label="Palette"
 *   options={[
 *     { value: 'aurora', label: 'Aurora Glass' },
 *     { value: 'ocean', label: 'Ocean Wave' },
 *   ]}
 *   value={selectedPalette}
 *   onChange={(value) => setSelectedPalette(value)}
 * />
 * ```
 */

export interface ListboxOption<T> {
  value: T;
  label: string;
  disabled?: boolean;
}

export interface ListboxProps<T> {
  options: ListboxOption<T>[];
  value: T | null;
  onChange: (value: T) => void;
  placeholder?: string;
  label: string;
  id?: string;
  disabled?: boolean;
  className?: string;
}

export function Listbox<T extends string | number>({
  options,
  value,
  onChange,
  placeholder = 'Select an option',
  label,
  id,
  disabled = false,
  className = '',
}: ListboxProps<T>): React.ReactElement {
  const [isOpen, setIsOpen] = useState(false);
  const [activeIndex, setActiveIndex] = useState(0);
  const listboxRef = useRef<HTMLUListElement>(null);
  const triggerRef = useRef<HTMLButtonElement>(null);
  const optionRefs = useRef<(HTMLLIElement | null)[]>([]);

  const reactId = useId().replace(/:/g, '');
  const listboxId = id || `listbox-${reactId}`;
  const triggerId = `${listboxId}-trigger`;

  // Find selected option
  const selectedOption = options.find(opt => opt.value === value);
  const displayText = selectedOption ? selectedOption.label : placeholder;

  // Update active index when value changes
  useEffect(() => {
    if (value !== null) {
      const index = options.findIndex(opt => opt.value === value);
      if (index >= 0) {
        setActiveIndex(index);
      }
    }
  }, [value, options]);

  // Close dropdown when clicking outside
  useEffect(() => {
    if (!isOpen) return;

    const handleClickOutside = (event: MouseEvent) => {
      const target = event.target as Node;
      if (
        listboxRef.current &&
        !listboxRef.current.contains(target) &&
        triggerRef.current &&
        !triggerRef.current.contains(target)
      ) {
        setIsOpen(false);
        triggerRef.current.focus();
      }
    };

    document.addEventListener('mousedown', handleClickOutside);
    return () => document.removeEventListener('mousedown', handleClickOutside);
  }, [isOpen]);

  // Scroll active option into view
  useEffect(() => {
    if (isOpen && optionRefs.current[activeIndex]) {
      optionRefs.current[activeIndex]?.scrollIntoView({
        block: 'nearest',
        behavior: 'smooth',
      });
    }
  }, [isOpen, activeIndex]);

  const handleTriggerKeyDown = (e: React.KeyboardEvent) => {
    if (disabled) return;

    if (!isOpen) {
      if (e.key === 'Enter' || e.key === ' ' || e.key === 'ArrowDown' || e.key === 'ArrowUp') {
        e.preventDefault();
        setIsOpen(true);
        // Set active index to selected value or first option
        const initialIndex = value !== null 
          ? options.findIndex(opt => opt.value === value)
          : 0;
        setActiveIndex(initialIndex >= 0 ? initialIndex : 0);
      }
      return;
    }

    // Handle keys when dropdown is open
    switch (e.key) {
      case 'Escape':
        e.preventDefault();
        setIsOpen(false);
        triggerRef.current?.focus();
        break;
      case 'Tab':
        // Allow Tab to close and move focus
        setIsOpen(false);
        break;
      case 'ArrowDown':
        e.preventDefault();
        setActiveIndex(prev => {
          let next = prev + 1;
          // Skip disabled options
          while (next < options.length && options[next].disabled) {
            next++;
          }
          return next < options.length ? next : prev;
        });
        break;
      case 'ArrowUp':
        e.preventDefault();
        setActiveIndex(prev => {
          let next = prev - 1;
          // Skip disabled options
          while (next >= 0 && options[next].disabled) {
            next--;
          }
          return next >= 0 ? next : prev;
        });
        break;
      case 'Home':
        e.preventDefault();
        {
          // Find first non-disabled option
          const firstIndex = options.findIndex(opt => !opt.disabled);
          if (firstIndex >= 0) {
            setActiveIndex(firstIndex);
          }
        }
        break;
      case 'End':
        e.preventDefault();
        {
          // Find last non-disabled option
          let lastIndex = options.length - 1;
          while (lastIndex >= 0 && options[lastIndex].disabled) {
            lastIndex--;
          }
          if (lastIndex >= 0) {
            setActiveIndex(lastIndex);
          }
        }
        break;
      case 'Enter':
      case ' ':
        e.preventDefault();
        if (activeIndex >= 0 && activeIndex < options.length && !options[activeIndex].disabled) {
          onChange(options[activeIndex].value);
          setIsOpen(false);
          triggerRef.current?.focus();
        }
        break;
    }
  };

  const handleOptionClick = (option: ListboxOption<T>) => {
    if (option.disabled) return;
    onChange(option.value);
    setIsOpen(false);
    triggerRef.current?.focus();
  };

  const handleOptionKeyDown = (e: React.KeyboardEvent, option: ListboxOption<T>) => {
    if (option.disabled) return;

    switch (e.key) {
      case 'Enter':
      case ' ':
        e.preventDefault();
        onChange(option.value);
        setIsOpen(false);
        triggerRef.current?.focus();
        break;
      case 'Escape':
        e.preventDefault();
        setIsOpen(false);
        triggerRef.current?.focus();
        break;
      default:
        // Delegate other keys to trigger handler
        handleTriggerKeyDown(e);
        break;
    }
  };

  return (
    <div className={`relative ${className}`}>
      <label htmlFor={triggerId} className="block text-[0.6875rem] font-medium mb-2">
        {label}
      </label>
      <div className="relative">
        <button
          ref={triggerRef}
          id={triggerId}
          type="button"
          onClick={() => !disabled && setIsOpen(!isOpen)}
          onKeyDown={handleTriggerKeyDown}
          disabled={disabled}
          aria-haspopup="listbox"
          aria-expanded={isOpen}
          aria-controls={listboxId}
          aria-labelledby={`${triggerId}-label`}
          className={`w-full flex items-center justify-between rounded-lg px-3 btn-secondary h-10 bg-[#2f3849] border border-white/5 text-xs text-left focus:outline-none focus:ring-2 focus:ring-primary focus:ring-offset-2 focus:ring-offset-background disabled:opacity-50 disabled:cursor-not-allowed ${
            isOpen ? 'border-primary/50' : ''
          }`}
        >
          <span id={`${triggerId}-label`} className={selectedOption ? '' : 'text-text-secondary'}>
            {displayText}
          </span>
          <ChevronDown 
            className={`w-3.5 h-3.5 text-text-secondary transition-transform ${isOpen ? 'rotate-180' : ''}`}
            aria-hidden="true"
          />
        </button>

        {isOpen && (
          <ul
            ref={listboxRef}
            id={listboxId}
            role="listbox"
            aria-labelledby={`${triggerId}-label`}
            className="absolute z-50 w-full mt-1 bg-[#2f3849] border border-white/10 rounded-lg shadow-lg max-h-60 overflow-auto focus:outline-none"
            style={{
              boxShadow: '0 8px 24px rgba(0,0,0,0.4)',
            }}
          >
            {options.map((option, index) => {
              const isSelected = option.value === value;
              const isActive = index === activeIndex;
              const optionId = `${listboxId}-option-${index}`;

              return (
                <li
                  key={String(option.value)}
                  ref={el => { optionRefs.current[index] = el; }}
                  id={optionId}
                  role="option"
                  aria-selected={isSelected}
                  aria-disabled={option.disabled}
                  tabIndex={-1}
                  onClick={() => handleOptionClick(option)}
                  onKeyDown={(e) => handleOptionKeyDown(e, option)}
                  className={`px-3 py-2 text-xs cursor-pointer transition-colors ${
                    option.disabled
                      ? 'opacity-50 cursor-not-allowed'
                      : 'hover:bg-white/5'
                  } ${
                    isActive && !option.disabled
                      ? 'bg-primary/20 text-primary'
                      : 'text-text-primary'
                  } ${
                    isSelected ? 'font-medium' : ''
                  }`}
                >
                  {option.label}
                </li>
              );
            })}
          </ul>
        )}
      </div>
    </div>
  );
}

