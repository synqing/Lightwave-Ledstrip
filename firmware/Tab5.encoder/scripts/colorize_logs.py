#!/usr/bin/env python3
import re
import sys

# Mapping of log tags to color constants
COLOR_MAP = {
    '[WDT]': 'COLOR_WDT',
    '[INIT]': 'COLOR_INIT',
    '[OK]': 'COLOR_OK',
    '[NETWORK]': 'COLOR_NETWORK',
    '[PRESET]': 'COLOR_PRESET',
    '[NVS]': 'COLOR_NVS',
    '[LED]': 'COLOR_LED',
    '[CoarseMode]': 'COLOR_COARSE',
    '[Button]': 'COLOR_BUTTON',
    '[TOUCH]': 'COLOR_TOUCH',
    '[I2C_RECOVERY]': 'COLOR_I2C_RECOVERY',
    '[LOOP_TRACE]': 'COLOR_LOOP_TRACE',
    '[STATUS]': 'COLOR_STATUS',
    '[WIFI]': 'COLOR_WIFI',
    '[WiFi]': 'COLOR_WIFI',
    '[DEBUG]': 'COLOR_DEBUG',
    '[UI]': 'COLOR_UI',
    '[DisplayUI_TRACE]': 'COLOR_DISPLAYUI_TRACE',
    '[ZC_TRACE]': 'COLOR_ZC_TRACE',
    '[ZoneComposer]': 'COLOR_ZONE',
    '[CT_TRACE]': 'COLOR_CT_TRACE',
    '[OTA]': 'COLOR_OTA',
    '[WS]': 'COLOR_WEBSOCKET',
}

def colorize_log_tags(content):
    """Replace log tags with colored versions"""
    for tag, color in COLOR_MAP.items():
        # Escape special regex characters in tag
        escaped_tag = re.escape(tag)
        
        # Pattern: "....[TAG]...." → "..." COLOR_XXX "[TAG]" ANSI_RESET "...."
        # This handles both println and printf cases
        pattern = r'"([^"]*?)' + escaped_tag + r'([^"]*?)"'
        
        def replacer(match):
            before = match.group(1)
            after = match.group(2)
            
            # Only colorize if the tag is at the start or after whitespace/newline
            if before == '' or before.endswith(' ') or before.endswith('\n') or before.endswith('\\n'):
                # Break the string and insert color macros
                if before:
                    result = f'"{before}" {color} "{tag}" ANSI_RESET "{after}"'
                else:
                    result = f'{color} "{tag}" ANSI_RESET "{after}"'
                return result
            else:
                return match.group(0)  # Don't colorize if tag is mid-sentence
        
        content = re.sub(pattern, replacer, content)
    
    return content

if __name__ == '__main__':
    if len(sys.argv) != 2:
        print("Usage: colorize_logs.py <file>")
        sys.exit(1)
    
    filepath = sys.argv[1]
    
    with open(filepath, 'r') as f:
        content = f.read()
    
    colorized = colorize_log_tags(content)
    
    with open(filepath, 'w') as f:
        f.write(colorized)
    
    print(f"Colorized {filepath}")
