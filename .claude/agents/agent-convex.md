---
name: agent-convex
description: Best practices for Convex backend implementation with proper CORS handling
model: inherit
color: blue
---

# Agent: Convex Backend Implementation

Best practices for implementing Convex backends with proper authentication, storage, and CORS handling.

## =4 CRITICAL: CORS Headers for Web Development

### Issue: Cross-Origin Requests Blocked
When developing Expo web apps (`npm run dev`), HTTP endpoints need CORS headers or browsers will block requests:

**ERROR:** `Access to fetch at 'https://your-deployment.convex.site/uploadImage' from origin 'http://localhost:8081' has been blocked by CORS policy`

### Solution: Always Include CORS Headers

```typescript
// L FAILS - No CORS headers
http.route({
  path: "/uploadImage", 
  method: "POST",
  handler: httpAction(async (ctx, request) => {
    const blob = await request.blob();
    const storageId = await ctx.storage.store(blob);
    return new Response(JSON.stringify({ storageId }));
  })
});

//  WORKS - With proper CORS headers
http.route({
  path: "/uploadImage",
  method: "POST", 
  handler: httpAction(async (ctx, request) => {
    // Define CORS headers
    const corsHeaders = {
      "Access-Control-Allow-Origin": "*",
      "Access-Control-Allow-Methods": "POST, OPTIONS",
      "Access-Control-Allow-Headers": "Content-Type, Authorization",
      "Access-Control-Max-Age": "86400",
    };

    // Handle preflight request
    if (request.method === "OPTIONS") {
      return new Response(null, {
        status: 200,
        headers: corsHeaders,
      });
    }

    // Verify authentication
    const authHeader = request.headers.get("Authorization");
    if (!authHeader || !authHeader.startsWith("Bearer ")) {
      return new Response("Unauthorized", {
        status: 401,
        headers: corsHeaders,
      });
    }

    const identity = await ctx.auth.getUserIdentity();
    if (!identity) {
      return new Response("Invalid token", {
        status: 401,
        headers: corsHeaders,
      });
    }

    try {
      const blob = await request.blob();
      const storageId = await ctx.storage.store(blob);
      
      return new Response(JSON.stringify({ storageId }), {
        status: 200,
        headers: {
          "Content-Type": "application/json",
          ...corsHeaders,
        },
      });
    } catch (error) {
      return new Response("Upload failed", {
        status: 500,
        headers: corsHeaders,
      });
    }
  }),
});

// Handle OPTIONS requests for CORS
http.route({
  path: "/uploadImage",
  method: "OPTIONS",
  handler: httpAction(async (ctx, request) => {
    return new Response(null, {
      status: 200,
      headers: {
        "Access-Control-Allow-Origin": "*",
        "Access-Control-Allow-Methods": "POST, OPTIONS",
        "Access-Control-Allow-Headers": "Content-Type, Authorization",
        "Access-Control-Max-Age": "86400",
      },
    });
  }),
});
```

## =� IMPORTANT: Storage URL Generation

### Always Use ctx.storage.getUrl()
Never manually construct URLs - always use Convex's storage API:

```typescript
// L FAILS - Manual URL construction
const imageUrl = `https://your-deployment.convex.site/api/storage/${storageId}`;

//  WORKS - Use Convex storage API
const imageUrl = await ctx.storage.getUrl(storageId);
if (!imageUrl) throw new Error('Failed to get storage URL');
```

### Frontend URL Fix for .convex.site
When uploading from frontend, replace `.convex.cloud` with `.convex.site`:

```typescript
// Frontend upload pattern
const handleImageUpload = async (imageUri: string) => {
  const convexUrl = process.env.EXPO_PUBLIC_CONVEX_URL;
  if (!convexUrl) throw new Error('Convex URL not configured');
  
  // CRITICAL: Replace .convex.cloud with .convex.site for HTTP endpoints
  const siteUrl = convexUrl.replace('.convex.cloud', '.convex.site');
  const uploadUrl = `${siteUrl}/uploadImage`;
  
  const response = await fetch(imageUri);
  const blob = await response.blob();
  const token = await getToken({ template: "convex" });
  
  const uploadResponse = await fetch(uploadUrl, {
    method: "POST",
    body: blob,
    headers: { 'Authorization': `Bearer ${token}` },
  });
  
  const { storageId } = await uploadResponse.json();
  return storageId;
};
```

## =� HELPFUL: Database Patterns

### Schema with Storage References
Always store storage IDs, not data URLs:

```typescript
// convex/schema.ts
export default defineSchema({
  projects: defineTable({
    userId: v.string(),
    name: v.string(),
    imageId: v.id("_storage"), // Storage reference
    imageUrl: v.string(), // Generated URL from ctx.storage.getUrl()
    createdAt: v.number(),
  }).index("by_user", ["userId"]),
});
```

### Query with URL Generation
Generate URLs dynamically in queries:

```typescript
// Get project with fresh image URL
export const getProject = query({
  args: { projectId: v.id("projects") },
  handler: async (ctx, { projectId }) => {
    const project = await ctx.db.get(projectId);
    if (!project) return null;
    
    // Generate fresh URL
    const imageUrl = await ctx.storage.getUrl(project.imageId);
    
    return {
      ...project,
      imageUrl, // Always fresh, never expired
    };
  },
});
```

### User-Scoped Queries
Always filter by user ID for security:

```typescript
export const listUserProjects = query({
  args: { userId: v.string() },
  handler: async (ctx, { userId }) => {
    // Verify authentication
    const identity = await ctx.auth.getUserIdentity();
    if (!identity || identity.subject !== userId) {
      throw new Error("Unauthorized");
    }
    
    return await ctx.db
      .query("projects")
      .withIndex("by_user", (q) => q.eq("userId", userId))
      .order("desc")
      .collect();
  },
});
```

## =� HELPFUL: Complete HTTP Template

Copy-paste template for authenticated file upload endpoints:

```typescript
import { httpRouter, httpAction } from "convex/server";

const http = httpRouter();

// CORS headers - reusable constant
const CORS_HEADERS = {
  "Access-Control-Allow-Origin": "*",
  "Access-Control-Allow-Methods": "POST, OPTIONS, GET",
  "Access-Control-Allow-Headers": "Content-Type, Authorization",
  "Access-Control-Max-Age": "86400",
};

// Generic file upload endpoint with authentication and CORS
http.route({
  path: "/uploadFile",
  method: "POST",
  handler: httpAction(async (ctx, request) => {
    // Handle preflight
    if (request.method === "OPTIONS") {
      return new Response(null, { status: 200, headers: CORS_HEADERS });
    }

    // Verify authentication
    const authHeader = request.headers.get("Authorization");
    if (!authHeader?.startsWith("Bearer ")) {
      return new Response("Unauthorized", { 
        status: 401, 
        headers: CORS_HEADERS 
      });
    }

    const identity = await ctx.auth.getUserIdentity();
    if (!identity) {
      return new Response("Invalid token", { 
        status: 401, 
        headers: CORS_HEADERS 
      });
    }

    try {
      // Process upload
      const blob = await request.blob();
      const storageId = await ctx.storage.store(blob);
      const url = await ctx.storage.getUrl(storageId);
      
      return new Response(JSON.stringify({ 
        success: true,
        storageId, 
        url 
      }), {
        status: 200,
        headers: {
          "Content-Type": "application/json",
          ...CORS_HEADERS,
        },
      });
    } catch (error) {
      console.error("Upload error:", error);
      return new Response(JSON.stringify({
        error: error instanceof Error ? error.message : "Upload failed"
      }), {
        status: 500,
        headers: {
          "Content-Type": "application/json", 
          ...CORS_HEADERS
        },
      });
    }
  }),
});

// OPTIONS handler
http.route({
  path: "/uploadFile",
  method: "OPTIONS", 
  handler: httpAction(async () => {
    return new Response(null, { status: 200, headers: CORS_HEADERS });
  }),
});

export default http;
```

## Environment Variables

Required for Convex + Clerk authentication:

```bash
# .env.local
EXPO_PUBLIC_CLERK_PUBLISHABLE_KEY=pk_test_*****
EXPO_PUBLIC_CONVEX_URL=https://your-deployment.convex.cloud
CLERK_JWT_ISSUER_DOMAIN=https://your-clerk-domain.clerk.accounts.dev
CONVEX_DEPLOYMENT=dev:your-deployment
```

**Note:** `CLERK_JWT_ISSUER_DOMAIN` must be set in Convex dashboard, not in `.env.local`.

## Quick Troubleshooting

| Error | Cause | Fix |
|-------|-------|-----|
| CORS policy blocked | Missing CORS headers | Add CORS headers to all responses |
| 401 Unauthorized | Missing/invalid auth token | Check Bearer token and Clerk config |
| 404 Not Found | Wrong endpoint URL | Use `.convex.site` for HTTP endpoints |
| Storage URL expired | Using cached URLs | Always use `ctx.storage.getUrl()` |
| Missing storageId | Frontend sending data URLs | Upload file first, then store ID |

## Testing Checklist

- [ ] Test with `npm run dev` (web) for CORS
- [ ] Test authentication with valid/invalid tokens  
- [ ] Test file upload >5MB
- [ ] Verify storage URLs are accessible
- [ ] Check user isolation (can't access other users' data)