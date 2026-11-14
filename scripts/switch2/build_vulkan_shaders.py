#!/usr/bin/env python3
"""Compile and validate the native video shaders; validate vendored UI SPIR-V."""
from pathlib import Path
import hashlib
import json
import re
import shutil
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[2]

def main():
    compiler = shutil.which('glslangValidator')
    validator = shutil.which('spirv-val')
    if not compiler or not validator:
        raise SystemExit('glslangValidator and spirv-val are required for native Vulkan shaders')
    output = ROOT / 'build/switch2-generated'
    output.mkdir(exist_ok=True)
    header = '#pragma once\n#include <cstdint>\n'
    record = {}
    for stage in ('vert', 'frag'):
        source = ROOT / ('app/src/streaming/video/Vulkan/video.' + stage)
        binary = output / ('video.' + stage + '.spv')
        subprocess.run([compiler, '-V', '--target-env', 'vulkan1.1', str(source), '-o', str(binary)], check=True)
        subprocess.run([validator, '--target-env', 'vulkan1.1', str(binary)], check=True)
        data = binary.read_bytes()
        words = struct.unpack('<' + 'I' * (len(data)//4), data)
        header += 'alignas(4) static constexpr uint32_t video_' + stage + '[] = {\n'
        for i in range(0, len(words), 8):
            header += ','.join(hex(v) for v in words[i:i+8]) + ',\n'
        header += '};\n'
        record[source.relative_to(ROOT).as_posix()] = hashlib.sha256(data).hexdigest()
    for stage in ('vert', 'frag'):
        source = ROOT / ('extern/borealis/library/lib/extern/nanovg-vulkan/src/shader/fill.' + stage + '.inc')
        words = [int(v, 16) for v in re.findall(r'0x[0-9a-fA-F]+', source.read_text())]
        binary = output / ('ui.' + stage + '.spv')
        data = struct.pack('<' + 'I' * len(words), *words)
        binary.write_bytes(data)
        subprocess.run([validator, '--target-env', 'vulkan1.1', str(binary)], check=True)
        record[source.relative_to(ROOT).as_posix()] = hashlib.sha256(data).hexdigest()
    (output / 'video_spirv.hpp').write_text(header)
    (output / 'shader-validation.json').write_text(json.dumps(record, indent=2) + '\n')
    fingerprint = hashlib.sha256()
    inputs = [ROOT / 'CMakeLists.txt', ROOT / 'extern/cmake/toolchain.cmake']
    for folder in ('app/src', 'scripts/switch2', 'extern/borealis/library/lib/extern/nanovg-vulkan'):
        inputs += [p for p in (ROOT / folder).rglob('*') if p.is_file() and '__pycache__' not in p.parts]
    for folder in ('extern/borealis/library/include/borealis/platforms/switch',
                   'extern/borealis/library/lib/platforms/switch'):
        inputs += [p for p in (ROOT / folder).rglob('*') if p.is_file()]
    inputs.append(ROOT / 'extern/borealis/library/include/borealis/core/frame_resources.hpp')
    for path in sorted(inputs):
        fingerprint.update(path.relative_to(ROOT).as_posix().encode())
        fingerprint.update(path.read_bytes())
    dependencies = {}
    for name, folder in (('borealis', 'extern/borealis'),
                         ('SDL3', 'extern/borealis/library/lib/extern/SDL3')):
        # Native Windows Git and MSYS Git must agree on CRLF checkouts.
        git = ['git', '-c', 'core.autocrlf=true', '-C', str(ROOT / folder)]
        revision = subprocess.check_output(git + ['rev-parse', 'HEAD'], text=True).strip()
        dependencies[name] = revision
        fingerprint.update(name.encode() + revision.encode())
        fingerprint.update(subprocess.check_output(git + ['diff', 'HEAD', '--']))
    build_id = fingerprint.hexdigest()[:20]
    (output / 'switch2_build_id.hpp').write_text('#define MOONLIGHT_SWITCH2_BUILD_ID "' + build_id + '"\n')
    (output / 'source-fingerprint.json').write_text(json.dumps({'buildId': build_id, 'dependencies': dependencies}, indent=2) + '\n')
    print('Native Vulkan video shaders compiled; video and UI SPIR-V validation passed')

if __name__ == '__main__':
    main()
