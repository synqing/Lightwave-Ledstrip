import re, sys, pathlib, json
from collections import defaultdict

registry_path = pathlib.Path(sys.argv[1])
out_dir = pathlib.Path(sys.argv[2])
repo_root = pathlib.Path(sys.argv[3])

text = registry_path.read_text(encoding="utf-8")

# Heuristic parser for Swift registry patterns like:
# static let connectionIndicator = RiveAssetDescriptor(
#   id: "connectionIndicator",
#   fileName: "lw_connection_indicator",
#   artboardName: "ConnectionIndicator",
#   stateMachineName: "Connection",
#   inputs: [...]
#   events: [...]
# )
asset_blocks = []
# Capture "static let <name> = ...(" blocks by slicing until next static let.
pattern = re.compile(r"static\s+let\s+(\w+)\s*=\s*([A-Za-z0-9_]+)\s*\(", re.S)
matches = list(pattern.finditer(text))
for idx, m in enumerate(matches):
    asset_id = m.group(1)
    start = m.end()
    end = matches[idx + 1].start() if idx + 1 < len(matches) else len(text)
    body = text[start:end]
    asset_blocks.append((asset_id, body))

def grab_string(label, body):
    m = re.search(rf"{label}\s*:\s*\"([^\"]+)\"", body)
    return m.group(1) if m else None

def grab_array(label, body):
    # matches label: [ ... ] including nested
    m = re.search(rf"{label}\s*:\s*\[(.*?)\]", body, re.S)
    if not m:
        return []
    inner = m.group(1)
    # extract quoted names conservatively
    return re.findall(r"\"([^\"]+)\"", inner)

def grab_inputs_with_types(body):
    matches = re.findall(r"\.(bool|number|text|trigger)\(\s*\"([^\"]+)\"", body)
    return [{"name": name, "type": kind} for kind, name in matches]

assets = []
for asset_id, body in asset_blocks:
    fileName = grab_string("fileName", body)
    artboardName = grab_string("artboardName", body)
    stateMachineName = grab_string("stateMachineName", body)
    # Inputs/events may be declared as typed descriptors; we grab quoted identifiers as best-effort.
    inputs = grab_inputs_with_types(body)
    events = grab_array("events", body)
    # Some registries use "tapEventName" or similar; keep it if present
    tapEventName = grab_string("tapEventName", body)

    # Skip blocks that don't look like a Rive asset descriptor
    if not fileName and not artboardName and not stateMachineName:
        continue

    assets.append({
        "id": asset_id,
        "fileName": fileName,
        "artboardName": artboardName,
        "stateMachineName": stateMachineName,
        "tapEventName": tapEventName,
        "inputs": inputs,
        "events": events
    })

if not assets:
    print("ERROR: No assets parsed. Update parser to match your registry format.", file=sys.stderr)
    sys.exit(2)

out_dir.mkdir(parents=True, exist_ok=True)

usage_map_path = out_dir / "_USAGE_MAP.md"
usage_md = usage_map_path.read_text(encoding="utf-8") if usage_map_path.exists() else ""

view_map_path = repo_root / "Docs" / "Rive" / "References" / "_VIEW_MAP.md"
view_map_md = view_map_path.read_text(encoding="utf-8") if view_map_path.exists() else ""

index_lines = ["# Rive Contract Index", "", f"Registry: `{registry_path}`", ""]
for a in assets:
    index_lines.append(f"- `{a['id']}` -> `{a['fileName']}.riv` | artboard `{a['artboardName']}` | sm `{a['stateMachineName']}`")
(out_dir / "_INDEX.md").write_text("\n".join(index_lines) + "\n", encoding="utf-8")

for a in assets:
    md = []
    md.append(f"# Contract: `{a['id']}`")
    md.append("")
    md.append("## Runtime file")
    md.append(f"- `.riv` filename (bundle): `{a['fileName']}.riv`" if a["fileName"] else "- `.riv` filename: (MISSING in registry)")
    md.append("")
    md.append("## Artboard / State Machine")
    md.append(f"- Artboard: `{a['artboardName']}`" if a["artboardName"] else "- Artboard: (MISSING)")
    md.append(f"- State Machine: `{a['stateMachineName']}`" if a["stateMachineName"] else "- State Machine: (MISSING)")
    if a["tapEventName"]:
        md.append(f"- Tap Event Name: `{a['tapEventName']}`")
    md.append("")
    md.append("## Inputs (contract names)")
    if a["inputs"]:
        for item in a["inputs"]:
            md.append(f"- `{item['name']}` ({item['type']})")
    else:
        md.append("- (none listed / parser could not detect; verify registry)")
    md.append("")
    md.append("## Events (contract names)")
    if a["events"]:
        for n in a["events"]:
            md.append(f"- `{n}`")
    else:
        md.append("- (none listed / parser could not detect; verify registry)")
    md.append("")
    md.append("## Where used in app")
    usage_section = None
    if usage_md:
        m = re.search(rf"## `{re.escape(a['id'])}`\n(.*?)(\n## |\Z)", usage_md, re.S)
        if m:
            usage_section = m.group(1).strip()
    if usage_section:
        md.append(usage_section)
    else:
        md.append("- (usage map not found)")

    md.append("")
    md.append("## View map")
    view_section = None
    if view_map_md:
        m = re.search(rf"## `{re.escape(a['id'])}`\n(.*?)(\n## |\Z)", view_map_md, re.S)
        if m:
            view_section = m.group(1).strip()
    if view_section:
        md.append(view_section)
    else:
        md.append("- (view map not found)")
    (out_dir / f"{a['id']}.md").write_text("\n".join(md) + "\n", encoding="utf-8")

print(f"OK: Generated {len(assets)} contract files in {out_dir}")
