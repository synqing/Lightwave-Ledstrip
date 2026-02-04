#!/usr/bin/env python3
import json
import subprocess
from pathlib import Path

def sips_size(path: Path) -> tuple[int, int]:
    result = subprocess.run(
        ["sips", "-g", "pixelWidth", "-g", "pixelHeight", str(path)],
        capture_output=True,
        text=True,
        check=True,
    )
    width = height = None
    for line in result.stdout.splitlines():
        line = line.strip()
        if line.startswith("pixelWidth:"):
            width = int(line.split(":", 1)[1].strip())
        elif line.startswith("pixelHeight:"):
            height = int(line.split(":", 1)[1].strip())
    if width is None or height is None:
        raise RuntimeError(f"Failed to parse size for {path}")
    return width, height


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    reports_dir = root / "reports" / "rive-reference"
    if not reports_dir.exists():
        raise SystemExit(f"Missing {reports_dir}. Run export first.")

    entries = []
    for png in sorted(reports_dir.glob("*.png")):
        name = png.stem
        width, height = sips_size(png)
        entry = {
            "artboard": name,
            "png": png.name,
            "size": {"width": width, "height": height},
        }
        if name.startswith("Screen_"):
            entry["type"] = "screen"
        elif name.startswith("Component_"):
            entry["type"] = "component"
        else:
            entry["type"] = "reference"
        entries.append(entry)

    manifest = {
        "generatedFrom": str(reports_dir),
        "count": len(entries),
        "entries": entries,
    }

    out_path = reports_dir / "ARTBOARD_PNG_MAP.json"
    out_path.write_text(json.dumps(manifest, indent=2))
    print(f"Wrote {out_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
