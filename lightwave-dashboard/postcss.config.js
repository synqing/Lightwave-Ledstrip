export default {
  plugins: {
    tailwindcss: {},
    autoprefixer: {},
    '@fullhuman/postcss-purgecss': process.env.NODE_ENV === 'production' ? {
      content: ['./index.html', './src/**/*.{js,ts,jsx,tsx}'],
      defaultExtractor: (content) => content.match(/[\w-/:]+(?<!:)/g) || [],
      safelist: {
        standard: [/^animate-/, /^bg-/, /^text-/, /^border-/],
        deep: [/^data-/, /^aria-/],
      },
    } : false,
  },
}
