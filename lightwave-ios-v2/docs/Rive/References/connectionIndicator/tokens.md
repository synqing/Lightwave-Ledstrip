# Tokens for connectionIndicator

## Files scanned
- `LightwaveOS/Views/Shared/PersistentStatusBar.swift`

## Font / typography
31:                        .font(.statusBrand)
65:                .font(.statusLabel)
70:                .font(.system(size: 10, weight: .semibold))
154:                .font(.cardLabel)
160:                .font(.monospace)

## Color usage
32:                        .foregroundStyle(Color.lwTextPrimary)
42:                .background(statusBarBackground)
66:                .foregroundStyle(Color.lwTextSecondary)
71:                .foregroundStyle(Color.lwTextTertiary)
104:            .fill(Color.lwCard)
147:        .background(drawerBackground)
155:                .foregroundStyle(Color.lwTextSecondary)
161:                .foregroundStyle(valueColor)
314:    .background(Color.lwBase)
320:        .background(Color.lwBase)

## Layout / shape
61:                .shadow(color: dotGlowColor, radius: 8, x: 0, y: 0)
103:        RoundedRectangle(cornerRadius: 12)
106:                RoundedRectangle(cornerRadius: 12)
107:                    .strokeBorder(Color.white.opacity(0.25), lineWidth: 2)
171:            UnevenRoundedRectangle(
179:            UnevenRoundedRectangle(
185:            .strokeBorder(Color.white.opacity(0.06), lineWidth: 1)
216:            return Color.lwSuccess.opacity(0.4)
218:            return Color.lwGold.opacity(0.3)

## Assets / images
69:            Image(systemName: isDrawerOpen ? "chevron.down" : "chevron.right")
