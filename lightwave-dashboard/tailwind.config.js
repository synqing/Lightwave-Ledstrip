/** @type {import('tailwindcss').Config} */
export default {
  content: [
    "./index.html",
    "./src/**/*.{js,ts,jsx,tsx}",
  ],
  theme: {
    extend: {
      fontFamily: {
        sans: ['Inter', 'system-ui', 'sans-serif'],
        display: ['"Bebas Neue"', 'sans-serif'],
      },
      colors: {
        background: '#0f1219',
        surface: '#252d3f',
        primary: {
          DEFAULT: '#ffb84d',
          dark: '#f59e0b',
        },
        accent: {
          cyan: '#6ee7f3',
          green: '#22dd88',
          red: '#ef4444',
        },
        text: {
          primary: '#e6e9ef',
          secondary: '#8b95a5',
        },
      },
      keyframes: {
        'pulse-glow': {
          '0%, 100%': { boxShadow: '0 0 8px rgba(34,221,136,0.5)' },
          '50%': { boxShadow: '0 0 16px rgba(34,221,136,0.8), 0 0 24px rgba(34,221,136,0.4)' },
        },
        'pulse-glow-red': {
          '0%, 100%': { boxShadow: '0 0 8px rgba(239,68,68,0.5)' },
          '50%': { boxShadow: '0 0 16px rgba(239,68,68,0.8), 0 0 24px rgba(239,68,68,0.4)' },
        },
        shake: {
          '0%, 100%': { transform: 'translateX(0)' },
          '25%': { transform: 'translateX(-2px)' },
          '75%': { transform: 'translateX(2px)' },
        },
        shimmer: {
          '0%': { backgroundPosition: '-200% 0' },
          '100%': { backgroundPosition: '200% 0' },
        },
        slideDown: {
          'from': { transform: 'translateY(-100%)' },
          'to': { transform: 'translateY(0)' },
        },
        borderGlow: {
          '0%, 100%': { borderColor: 'rgba(239,68,68,0.4)' },
          '50%': { borderColor: 'rgba(239,68,68,0.8)' },
        },
      },
      animation: {
        'pulse-glow': 'pulse-glow 2s ease-in-out infinite',
        'pulse-glow-red': 'pulse-glow-red 1s ease-in-out infinite',
        'shake': 'shake 0.3s ease-in-out',
        'shimmer': 'shimmer 1.5s infinite',
        'slide-down': 'slideDown 0.3s ease-out',
        'border-glow': 'borderGlow 2s ease-in-out infinite',
      },
    },
  },
  plugins: [],
}
