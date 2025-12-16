---
name: agent-vercel
description: Best practices for Vercel deployment and production shipping
model: inherit
color: black
---

# Agent: Vercel Deployment

Best practices for deploying applications with Vercel, including production deployments, environment configuration, and common deployment patterns.

## 🚀 CRITICAL: Production Deployment

### Basic Deployment Commands

```bash
# Preview deployment (creates unique URL)
vercel

# Production deployment
vercel --prod

# Deploy with specific configuration
vercel --prod --yes  # Skip confirmation prompts
```

### Common Deployment Issues & Solutions

**ERROR:** `Error: No project linked. Run 'vercel link' to connect to a project`

**Solution:** Link your project first:
```bash
vercel link
# Follow prompts to connect to existing or create new project
```

**ERROR:** `Build failed: Module not found`

**Solution:** Ensure all dependencies are in package.json:
```bash
# Install missing dependencies
npm install

# Commit package.json and package-lock.json
git add package.json package-lock.json
git commit -m "Update dependencies"

# Deploy again
vercel --prod
```

## 🔧 IMPORTANT: Environment Variables

### Setting Environment Variables

```bash
# Add environment variable for all environments
vercel env add SECRET_KEY

# Add for specific environments
vercel env add DATABASE_URL production
vercel env add DATABASE_URL development
vercel env add DATABASE_URL preview

# Pull environment variables to .env.local
vercel env pull
```

### Environment Variable Patterns

```bash
# Production secrets
vercel env add DATABASE_URL production
vercel env add API_SECRET production

# Public variables (accessible in browser)
vercel env add NEXT_PUBLIC_API_URL
vercel env add NEXT_PUBLIC_SITE_URL
```

## 📦 HELPFUL: Build Configuration

### vercel.json Configuration

```json
{
  "buildCommand": "npm run build",
  "outputDirectory": "dist",
  "devCommand": "npm run dev",
  "installCommand": "npm install",
  "framework": "nextjs",
  "regions": ["iad1"],
  "functions": {
    "app/api/*": {
      "maxDuration": 10
    }
  },
  "redirects": [
    {
      "source": "/old-path",
      "destination": "/new-path",
      "permanent": true
    }
  ],
  "headers": [
    {
      "source": "/api/(.*)",
      "headers": [
        {
          "key": "Access-Control-Allow-Origin",
          "value": "*"
        }
      ]
    }
  ],
  "env": {
    "NODE_ENV": "production"
  }
}
```

### Next.js Specific Configuration

```json
{
  "framework": "nextjs",
  "buildCommand": "next build",
  "devCommand": "next dev -p 3000",
  "outputDirectory": ".next",
  "regions": ["iad1"],
  "functions": {
    "app/api/route.ts": {
      "maxDuration": 60,
      "memory": 1024
    }
  }
}
```

## 🎯 HELPFUL: Deployment Workflow

### Complete Deployment Process

```bash
# 1. Install Vercel CLI (if not installed)
npm i -g vercel

# 2. Login to Vercel
vercel login

# 3. Link project (first time)
vercel link

# 4. Set environment variables
vercel env add DATABASE_URL production
vercel env add NEXT_PUBLIC_API_URL

# 5. Test with preview deployment
vercel

# 6. Deploy to production
vercel --prod

# 7. Check deployment status
vercel ls

# 8. View logs
vercel logs
```

### Domain Management

```bash
# Add custom domain
vercel domains add example.com

# List domains
vercel domains ls

# Remove domain
vercel domains rm example.com

# Inspect domain configuration
vercel domains inspect example.com
```

## 🔍 HELPFUL: Debugging & Monitoring

### Viewing Logs

```bash
# View recent logs
vercel logs

# View specific deployment logs
vercel logs [deployment-url]

# Follow logs in real-time
vercel logs --follow

# Filter by function
vercel logs --filter="app/api/user"
```

### Rollback Deployments

```bash
# List all deployments
vercel ls

# Promote previous deployment to production
vercel promote [deployment-url]

# Remove deployment
vercel rm [deployment-url]
```

## ⚡ HELPFUL: Performance Optimization

### Edge Functions

```typescript
// app/api/edge-function/route.ts
export const runtime = 'edge'; // Use Edge Runtime

export async function GET(request: Request) {
  return new Response('Hello from the Edge!', {
    status: 200,
    headers: {
      'content-type': 'text/plain',
    },
  });
}
```

### Image Optimization

```javascript
// next.config.js
module.exports = {
  images: {
    domains: ['example.com'],
    formats: ['image/avif', 'image/webp'],
  },
};
```

## 🛠️ HELPFUL: Common Commands Reference

| Command | Description |
|---------|------------|
| `vercel` | Deploy preview |
| `vercel --prod` | Deploy to production |
| `vercel dev` | Run development server |
| `vercel build` | Build project locally |
| `vercel env pull` | Download environment variables |
| `vercel env ls` | List environment variables |
| `vercel domains ls` | List domains |
| `vercel logs` | View deployment logs |
| `vercel ls` | List deployments |
| `vercel inspect [url]` | Inspect deployment |
| `vercel rm [url]` | Remove deployment |
| `vercel whoami` | Show current user |
| `vercel logout` | Logout from CLI |
| `vercel alias` | Manage deployment aliases |
| `vercel secrets` | Manage secrets (deprecated, use env) |

## 🚨 Quick Troubleshooting

| Error | Cause | Fix |
|-------|-------|-----|
| Build failed | Missing dependencies | Check package.json and install |
| 404 after deploy | Wrong output directory | Check vercel.json configuration |
| Environment variables undefined | Not set for environment | Use `vercel env add` for all environments |
| Function timeout | Execution too long | Increase maxDuration in vercel.json |
| CORS errors | Missing headers | Add headers in vercel.json |
| No project linked | Not connected to Vercel | Run `vercel link` |

## Testing Checklist

- [ ] Preview deployment works (`vercel`)
- [ ] Production deployment works (`vercel --prod`)
- [ ] Environment variables are set correctly
- [ ] Custom domain is configured (if applicable)
- [ ] API routes respond correctly
- [ ] Static assets load properly
- [ ] Redirects work as expected
- [ ] Edge functions deploy (if used)