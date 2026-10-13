#!/usr/bin/env bash
# Install the user-level desktop/menu launcher and icons; no sudo is needed.
set -euo pipefail
script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
if [[ $# -gt 1 ]]; then echo "Usage: $0 [runtime-directory]" >&2; exit 2; fi
if [[ $# -eq 1 ]]; then
    runtime_dir="$(cd -- "$1" && pwd -P)"
elif [[ -f "$script_dir/run.sh" ]]; then
    runtime_dir="$script_dir"
else
    runtime_dir="$(cd -- "$script_dir/../release/ubuntu-x86_64" && pwd -P)"
fi
[[ -x "$runtime_dir/run.sh" && -x "$runtime_dir/ShDisplayViewer" ]] || {
    echo "The runtime directory needs executable run.sh and ShDisplayViewer." >&2; exit 1;
}
data_root="${XDG_DATA_HOME:-$HOME/.local/share}"
applications_dir="$data_root/applications"
theme_dir="$data_root/icons/hicolor"
desktop_dir="$(xdg-user-dir DESKTOP)"
[[ -n "$desktop_dir" && -d "$desktop_dir" ]] || { echo "Desktop directory is unavailable." >&2; exit 1; }
for size in 16 24 32 48 64 128 256 512; do
    [[ -f "$runtime_dir/icons/shdisplayviewer-$size.png" ]] || { echo "Missing application icon size $size." >&2; exit 1; }
done
mkdir -p "$applications_dir" "$theme_dir/scalable/apps"
for size in 16 24 32 48 64 128 256 512; do
    mkdir -p "$theme_dir/${size}x${size}/apps"
    install -m 644 "$runtime_dir/icons/shdisplayviewer-$size.png" "$theme_dir/${size}x${size}/apps/shdisplayviewer.png"
done
install -m 644 "$runtime_dir/icons/shdisplayviewer.svg" "$theme_dir/scalable/apps/shdisplayviewer.svg"

backup_dir="$runtime_dir/log/launcher-backups/$(date +%Y%m%d-%H%M%S)"
mkdir -p "$backup_dir"
[[ ! -f "$applications_dir/ShDisplayViewer.desktop" ]] || cp -p "$applications_dir/ShDisplayViewer.desktop" "$backup_dir/menu.desktop"
[[ ! -f "$desktop_dir/ShDisplayViewer.desktop" ]] || cp -p "$desktop_dir/ShDisplayViewer.desktop" "$backup_dir/desktop.desktop"
python3 - "$runtime_dir" "$applications_dir/ShDisplayViewer.desktop" <<'PY'
import pathlib, sys
runtime, target = sys.argv[1:]
def value(text):
    return text.replace('\\', '\\\\').replace('\n', '\\n').replace('\r', '\\r').replace('\t', '\\t')
def executable(text):
    text = text.replace('%', '%%')
    for char in ('\\', '"', '`', '$'):
        text = text.replace(char, '\\' + char)
    return '"' + text + '"'
entry = '\n'.join([
    '[Desktop Entry]', 'Version=1.0', 'Type=Application',
    'Name=ShDisplayViewer-三维地图演练系统',
    'Name[zh_CN]=ShDisplayViewer-三维地图演练系统',
    'Comment=三维地图部署与步骤演练',
    'Exec=' + executable(runtime + '/run.sh'),
    'TryExec=' + value(runtime + '/run.sh'),
    'Path=' + value(runtime), 'Icon=shdisplayviewer', 'Terminal=false',
    'Categories=Graphics;', 'StartupNotify=true',
    'StartupWMClass=ShDisplayViewer', ''
])
pathlib.Path(target).write_text(entry, encoding='UTF-8')
PY
chmod 755 "$applications_dir/ShDisplayViewer.desktop"
install -m 755 "$applications_dir/ShDisplayViewer.desktop" "$desktop_dir/ShDisplayViewer.desktop"
install -m 755 "$applications_dir/ShDisplayViewer.desktop" "$runtime_dir/ShDisplayViewer.desktop"
if command -v desktop-file-validate >/dev/null; then
    desktop-file-validate "$applications_dir/ShDisplayViewer.desktop"
fi
if command -v update-desktop-database >/dev/null; then update-desktop-database "$applications_dir"; fi
if command -v gtk-update-icon-cache >/dev/null; then gtk-update-icon-cache -f -t "$theme_dir"; fi
if command -v gio >/dev/null && gio set "$desktop_dir/ShDisplayViewer.desktop" metadata::trusted true; then
    echo "Desktop launcher is trusted and ready."
else
    echo "Launcher installed; this desktop may require right-click -> Allow Launching." >&2
fi
echo "Desktop shortcut: $desktop_dir/ShDisplayViewer.desktop"
