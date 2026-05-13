# LightwaveOS Trademark Policy

This document describes the trademark and branding guidelines for the LightwaveOS project. The goal is simple: protect the project's identity so that users always know what is official, while making sure the community can talk about, build on, and extend LightwaveOS freely.

This policy is meant to be read alongside the [Apache License 2.0](LICENSE), which governs the source code. The Apache License explicitly states (Section 6) that it does not grant permission to use the trade names, trademarks, or service marks of the licensor. This document fills that gap.

---

## Trademarks owned by SpectraSynq

The following names and marks are trademarks or unregistered trademarks of SpectraSynq:

| Mark | Status | Notes |
|------|--------|-------|
| **LightwaveOS** | Unregistered trademark | The name of this firmware project |
| **SpectraSynq** | Unregistered trademark | The organisation behind LightwaveOS |

**"Light Guide Plate" (LGP)** is a generic industry term for edge-lit acrylic optical panels. It is not claimed as a trademark by SpectraSynq or anyone else -- use it freely.

### Patent notice

SpectraSynq has a pending patent application covering audio-to-light AI classification technology used in this project. The Apache 2.0 licence includes a patent grant for contributors and users of the licensed code (see Section 3 of the [LICENSE](LICENSE)). This trademark policy does not affect patent rights in any way.

---

## Permitted uses (no permission needed)

You are welcome to do any of the following without asking:

### Referring to the project

- Use "LightwaveOS" to accurately describe this project or your relationship to it.
  - *"This library is compatible with LightwaveOS."*
  - *"I built a custom enclosure for my LightwaveOS controller."*
  - *"Based on LightwaveOS firmware v3."*

### Describing forks and derivatives

- State that your work is derived from or based on LightwaveOS.
  - *"Forked from LightwaveOS"*
  - *"Built on the LightwaveOS audio pipeline"*

### Crediting the project

- Use "Powered by LightwaveOS" in your project's documentation, README, or about screen to indicate that your hardware runs LightwaveOS firmware.
- Include the standard attribution notices required by the Apache 2.0 licence.

### Community and editorial use

- Write blog posts, tutorials, reviews, or conference talks that mention LightwaveOS by name.
- Use "LightwaveOS" in academic papers, benchmarks, or comparative analyses.

### Package and integration references

- Use "lightwaveos" (lowercase, no spaces) in package manager identifiers, library names, or dependency lists where the reference clearly points back to this project.
  - *`lightwaveos-effects-sdk`* as a plugin package for the official project is fine.

---

## Restricted uses (permission required)

The following uses require written permission from SpectraSynq. This is not about being territorial -- it is about preventing confusion over what is official.

### Using the marks in product or project names

- Do not use "LightwaveOS" or "SpectraSynq" as part of your product name, company name, domain name, or social media handle in a way that suggests official status.
  - **Not OK**: "LightwaveOS Pro", "LightwaveOS Cloud", "SpectraSynq Audio"
  - **OK**: "Aurora LED Controller (compatible with LightwaveOS)"

### Implying endorsement or affiliation

- Do not use the marks in a way that implies SpectraSynq has endorsed, sponsored, or is affiliated with your project, product, or organisation -- unless that is actually the case.

### Modified versions distributed under the original name

- If you distribute a modified version of LightwaveOS, you must not call it "LightwaveOS" without clearly distinguishing it from the official release. See the fork naming guidelines below.

### Merchandise and physical goods

- Do not use "LightwaveOS" or "SpectraSynq" on merchandise, printed circuit boards, product packaging, or physical goods without permission.

---

## Fork naming guidelines

Forking is encouraged -- that is the point of open source. However, please follow these naming rules so users can tell official releases from community variants:

### What to do

1. **Choose your own name** for your fork. Be creative.
   - *"Photonwave"*, *"LGP-Remix"*, *"AuroraStrip"* -- all fine.
2. **Credit the upstream project** in your README and NOTICE file.
   - *"Forked from [LightwaveOS](https://github.com/synqing/Lightwave-Ledstrip), copyright 2025-2026 SpectraSynq, licensed under Apache 2.0."*
3. **Keep the Apache 2.0 attribution** as required by the licence.

### What not to do

- Do not name your fork "LightwaveOS" or any confusingly similar variation (e.g., "LightwaveOS-2", "Lightwave-OS", "LW-OS by [YourName]").
- Do not use "SpectraSynq" in your fork's name.
- Do not use a name that suggests your fork is the official or endorsed version.

### Grey area

If you are maintaining a long-lived community fork and want to include "LightwaveOS" in the name for clarity (e.g., "LightwaveOS Community Edition"), please reach out first. We are generally happy to grant permission for good-faith community efforts -- we just want to coordinate.

---

## Logo usage

Official logo assets and usage guidelines will be provided in a separate document once the project's visual identity is finalised. Until then:

- Do not create or distribute logos that use the "LightwaveOS" or "SpectraSynq" word marks in a stylised form that could be mistaken for an official logo.
- Simple text references (as described in the permitted uses section) are always fine.

This section will be updated with links to logo files, colour specifications, and clear-space requirements when they are available.

---

## Questions and permissions

For trademark questions, permission requests, or if you are unsure whether your use is covered by this policy:

- **Open a GitHub Issue** on the [LightwaveOS repository](https://github.com/synqing/Lightwave-Ledstrip) with the label `trademark`.
- Alternatively, reach out to the maintainers through the channels listed in the repository's README.

We aim to respond within a reasonable timeframe. If your use is clearly in good faith, the answer is almost certainly yes.

---

## Changes to this policy

This policy may be updated from time to time. Changes will be documented in the project's changelog and committed to the repository. The version in the `main` branch of the repository is always the current version.

---

*This trademark policy is inspired by the approaches used by the Rust, Node.js, and Mozilla projects, adapted for a small open-source hardware/firmware project. It is not legal advice.*
