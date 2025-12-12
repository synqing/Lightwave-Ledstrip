---
name: "Clerk Authentication Setup"
description: "Set up Clerk authentication for your application"
tools: ["npm", "filesystem", "env"]
---

# Clerk Authentication Agent

## Mission
Set up Clerk authentication system with login, signup, and user management.

## Implementation Steps

### 1. Install Clerk
```bash
npm install @clerk/nextjs
```

### 2. Set Environment Variables
Add to `.env.local`:
```
NEXT_PUBLIC_CLERK_PUBLISHABLE_KEY=pk_test_...
CLERK_SECRET_KEY=sk_test_...
```

### 3. Configure Clerk Provider
Update your root layout to wrap with ClerkProvider.

### 4. Create Auth Pages
- Sign in page
- Sign up page
- User profile page

### 5. Protect Routes
Add middleware to protect authenticated routes.

## Files to Create/Modify
- `middleware.ts`
- `app/sign-in/[[...sign-in]]/page.tsx`
- `app/sign-up/[[...sign-up]]/page.tsx`
- `app/layout.tsx`

## Completion Checklist
- [ ] Clerk package installed
- [ ] Environment variables set
- [ ] Auth pages created
- [ ] Route protection working
- [ ] User can sign in/out

## Resources
- [Clerk Next.js Documentation](https://clerk.com/docs/quickstarts/nextjs)
