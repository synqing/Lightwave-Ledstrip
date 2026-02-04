# Rive Design Source Handoff

This folder is the designer handoff pack for Rive assets. Follow these requirements exactly.

## Naming and Contracts (Required)
- Artboard name must match the contract in `Docs/Rive/Contract/<asset_id>.md`.
- State machine name must match the contract in `Docs/Rive/Contract/<asset_id>.md`.
- Inputs must exist with the exact names and types defined in the contract.
- Any events listed in the contract must exist with the exact names.

## Export Rules (Required)
- Export runtime file as `.riv` named exactly `<fileName>.riv`.
- Store the `.riv` file in `LightwaveOS/Resources/Rive/`.
- Commit the source project file (`.rev`) alongside the `.riv` file in the same folder.
- No source file means no future edits, so `.rev` is mandatory.

## Asset Creation Guidance
- Use embedded assets initially (no external dependencies).
- Match the look and layout from `Docs/Rive/References/<asset_id>/`.
- Use the token and motion references to recreate type, colours, and motion.

## Validation Checklist
- Open the exported `.riv` in the Rive viewer and confirm it matches contract names.
- Confirm inputs respond and events fire as expected.
- Once assets are delivered, run the app tests (`RiveAssetTests`) to validate.
