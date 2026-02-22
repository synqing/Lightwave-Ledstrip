# aura.build Prompt 4: Connection & Onboarding Flow

## Attachments (2 max)
1. **Screenshot of current ConnectionSheet** → `../01_connection_sheet.png`
2. **DESIGN_SPEC.md** → `../reference/DESIGN_SPEC.md`

## Prompt

Redesign this device connection sheet for an iOS LED controller app.
The app discovers ESP32-S3 LED controllers via Bonjour/mDNS on local network.

Current flow: Modal sheet shows discovered devices list + manual IP entry.
Three discovery tiers: automatic mDNS, HTTP probe, and Access Point mode.

Improve:
- Add animated scanning indicator during discovery (pulsing radar rings)
- Show device thumbnails with connection quality indicators (RSSI bars)
- Add "Recently connected" section at top for quick reconnection
- Access Point setup instructions with step-by-step illustrations
- Manual IP entry as expandable "Advanced" section
- Connection progress: targeting -> handshake -> connecting -> ready with animated steps
- Error states with clear recovery actions ("Try Again", "Switch to Manual")

Brand: LightwaveOS, gold (#FFB84D) accent on dark (#0F1219) background.
Premium glass morphism cards. Bebas Neue headings, Rajdhani body text.
iPhone 15 Pro frame.

## Design Goals
- First-time user experience: clear, guided, non-technical
- Scanning animation that shows the app is actively looking
- Device cards with hostname, IP, signal strength
- "Recently Connected" section for returning users
- Access Point mode with illustrated step-by-step setup guide
- Connection progress indicator showing each handshake phase
- Error recovery with contextual suggestions
- Manual IP entry hidden under "Advanced" expandable section

## Current Issues Being Addressed
- Connection sheet is functional but not polished
- Error display uses same card style as normal content (no icon differentiation)
- No "recently connected" for quick reconnection
- AP mode instructions are text-only (no illustrations)
- No animated scanning indicator during discovery
- Connection progress is not visible (black box until success/failure)

## Connection State Machine Reference
```
idle -> targeting -> handshakeHTTP -> connectingWS -> ready
                                                  -> backoff(attempt, nextRetry)
                                                  -> failed(reason)
```
