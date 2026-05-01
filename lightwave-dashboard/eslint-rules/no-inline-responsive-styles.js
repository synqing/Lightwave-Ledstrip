/**
 * ESLint rule: no-inline-responsive-styles
 * 
 * Prevents invalid Tailwind responsive prefixes in inline style attributes.
 * 
 * Example violations:
 * - style="width:10rem; sm:width:12rem" ❌
 * - style={{ width: '10rem', 'sm:width': '12rem' }} ❌
 * 
 * Should use Tailwind classes instead:
 * - className="w-40 sm:w-48" ✅
 */

export default {
  meta: {
    type: 'problem',
    docs: {
      description: 'Disallow Tailwind responsive prefixes in inline styles',
      category: 'Best Practices',
      recommended: true,
    },
    messages: {
      invalid: 'Inline styles cannot use Tailwind responsive prefixes (sm:, md:, lg:, xl:, 2xl:). Use className with Tailwind classes instead.',
    },
    schema: [],
  },
  create(context) {
    // Pattern to match responsive prefixes in style values
    const responsivePattern = /(sm|md|lg|xl|2xl):/;
    
    return {
      JSXAttribute(node) {
        // Check for style attribute
        if (node.name.name !== 'style') {
          return;
        }
        
        // Handle string literal style values
        if (node.value?.type === 'Literal' && typeof node.value.value === 'string') {
          const styleValue = node.value.value;
          if (responsivePattern.test(styleValue)) {
            context.report({
              node: node.value,
              messageId: 'invalid',
            });
          }
        }
        
        // Handle JSX expression style values (style={{ ... }})
        if (node.value?.type === 'JSXExpressionContainer') {
          const expression = node.value.expression;
          
          // Object expression: style={{ width: '10rem', 'sm:width': '12rem' }}
          if (expression.type === 'ObjectExpression') {
            for (const property of expression.properties) {
              if (property.type === 'Property' && property.key) {
                const keyName = property.key.name || property.key.value;
                if (typeof keyName === 'string' && responsivePattern.test(keyName)) {
                  context.report({
                    node: property.key,
                    messageId: 'invalid',
                  });
                }
              }
            }
          }
        }
      },
    };
  },
};

