#!/usr/bin/env python3
"""Export reviewed source files only. Never publishes or accesses accounts."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
ROOT_FILES = ('CMakeLists.txt','CMakePresets.json','README.md','LICENSE','NOTICE','AUTHORS.md',
              'THIRD_PARTY_NOTICES.md','CONTRIBUTING.md','CHANGELOG.md','.gitignore','.gitattributes')
SOURCE_DIRS = ('src','tests','resources','assets','.github')
SDK_FILES = ('sdk/ShEarthSDK/README.md','sdk/ShEarthSDK/include/ShEarthSDK/EarthRuntime.h',
             'sdk/ShVideoSDK/README.md','sdk/ShVideoSDK/include/ShVideoSDK/VideoPlayer.h')
DOCS = ('DESIGN.md','BUILD.md','ACCEPTANCE.md','PUBLISHING.md','VALIDATION.md','CONFIGURATION.md',
        'USER_GUIDE.md','USER_GUIDE.html','USER_GUIDE.pdf','UBUNTU_RELEASE.md','ASSET_PROVENANCE.md',
        'SDK_VERSION_MATRIX.md','SDK_LAYOUT.md','SDK_VIEWER_TERMS_DRAFT.md','GITHUB_SETUP.md','images/interface-overview.svg')
SCRIPTS = ('Build-Windows.cmd','Deploy-Windows.ps1','Run-Windows.cmd','build-ubuntu.sh','run-ubuntu.sh',
           'Prepare-Release.cmake','check-public.py','export-public.py','Install-Ubuntu-Shortcut.sh','render-app-icon.cjs')
EXAMPLES = ('examples/warehouse-fire/README.md','examples/warehouse-fire/dynamic-plans.xml','examples/warehouse-fire/generate.py')
SUFFIXES = {'.c','.cc','.cpp','.cxx','.h','.hpp','.ui','.qrc','.cmake','.in','.md','.txt','.png','.jpg',
            '.tga','.svg','.ico','.cjs','.ive','.xml','.earth','.json','.py','.ps1','.cmd','.sh','.pri','.yml','.yaml'}
PRUNE = {'private','dependencies','build','dist','release','cache','cache_flat','log','settings','Projects',
         'map','maps','tiles','ffmpeg7','win64','__pycache__','.git'}
EXCLUDE_DIRS = {'resources/flight/H6','resources/images/login','resources/images/menu',
                'sdk/ShEarthSDK/windows-x64','sdk/ShEarthSDK/ubuntu2204-x64',
                'sdk/ShVideoSDK/windows-x64','sdk/ShVideoSDK/ubuntu2204-x64'}

def selected_files():
    selected = [ROOT / name for name in ROOT_FILES]
    # 20261014 hello: only reviewed public SDK facades may enter the source export.
    # Do not walk SDK trees, even if a future private file has an allowed suffix.
    selected += [ROOT/name for name in SDK_FILES]
    selected += [ROOT/'docs'/name for name in DOCS] + [ROOT/'scripts'/name for name in SCRIPTS]
    selected += [ROOT/name for name in EXAMPLES]
    for name in SOURCE_DIRS:
        start = ROOT/name
        if start.is_symlink() or not start.is_dir():
            raise RuntimeError(f'Missing or symlink source directory: {name}')
        for directory, dirs, files in os.walk(start, followlinks=False):
            current = Path(directory)
            kept=[]
            for child in dirs:
                path=current/child
                if path.is_symlink(): raise RuntimeError(f'Symlink: {path.relative_to(ROOT)}')
                if child not in PRUNE and path.relative_to(ROOT).as_posix() not in EXCLUDE_DIRS: kept.append(child)
            dirs[:] = kept
            for child in files:
                path=current/child
                if path.is_symlink(): raise RuntimeError(f'Symlink: {path.relative_to(ROOT)}')
                if child.startswith('._') or child=='.DS_Store': continue
                if path.suffix.lower() in SUFFIXES or child=='CMakeLists.txt': selected.append(path)
    selected=sorted(set(selected))
    for path in selected:
        if path.is_symlink() or not path.is_file(): raise RuntimeError(f'Missing or symlink file: {path.relative_to(ROOT)}')
    return selected

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--name',default='source-candidate')
    args=parser.parse_args()
    if not args.name or any(c not in 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_' for c in args.name):
        parser.error('name must use letters, digits, - or _')
    destination=ROOT/'exports'/args.name
    if destination.exists(): parser.error('destination exists; choose a new name')
    manifest={}
    for source in selected_files():
        relative=source.relative_to(ROOT);output=destination/relative
        output.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,output)
        manifest[relative.as_posix()]=hashlib.sha256(output.read_bytes()).hexdigest()
    (destination/'SOURCE-MANIFEST.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
    subprocess.run([sys.executable,str(ROOT/'scripts/check-public.py'),str(destination)],check=True)
    print(f'Exported source candidate: {destination} ({len(manifest)} files)')

if __name__=='__main__': main()
