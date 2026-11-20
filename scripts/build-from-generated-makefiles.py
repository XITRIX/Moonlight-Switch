"""Continue a configured devkitPro build using native Windows tools only.

Reuse CMake's generated compile/link commands without launching MSYS2 make.
Requires build/switch-local to have been configured already, and GLM headers
under the workspace's .local-tools/glm. All subprocesses use argv, not a shell.
"""
import concurrent.futures
import os
from pathlib import Path
import re
import shlex
import shutil
import struct
import sys
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build/switch-local'
SDK = Path('C:/devkitPro')
GLM = ROOT.parent / '.local-tools/glm'
LOG = ROOT / 'build/native-windows.log'
ENV = os.environ.copy()
ENV['DEVKITPRO'] = SDK.as_posix()
ENV['DEVKITA64'] = (SDK / 'devkitA64').as_posix()
TLS_ABI_STAMP = BUILD / 'soft-local-exec-tls-abi-v2.stamp'


def has_tls_relocations(path):
    """Identify old objects that must be rebuilt after the TLS ABI correction."""
    with path.open('rb') as f:
        header = f.read(64)
        if len(header) < 64 or header[:5] != b'\x7fELF\x02': return False
        offset = struct.unpack_from('<Q', header, 40)[0]
        size, count = struct.unpack_from('<HH', header, 58)
        f.seek(offset)
        sections = f.read(size * count)
        headers = [struct.unpack_from('<IIQQQQIIQQ', sections, i * size) for i in range(count)]
        for i in range(count):
            section = headers[i]
            if section[1] == 2 and section[9] == 24:
                strings = headers[section[6]]
                f.seek(strings[4]); names = f.read(strings[5])
                f.seek(section[4])
                for name, info, _, _, _, _ in struct.iter_unpack('<IBBHQQ', f.read(section[5])):
                    if info & 15 == 6 or names[name:names.find(b'\0', name)] == b'__aarch64_read_tp':
                        return True
            if section[1] != 4 or section[9] != 24: continue
            f.seek(section[4])
            for _, info, _ in struct.iter_unpack('<QQq', f.read(section[5])):
                if 512 <= (info & 0xffffffff) <= 573: return True
    return False


def native(value):
    return value.replace('/opt/devkitpro', SDK.as_posix()).replace('/c/', 'C:/')


def run(argv, cwd):
    result = subprocess.run(argv, cwd=cwd, env=ENV, capture_output=True, text=True, errors='replace')
    if result.returncode:
        raise RuntimeError(' '.join(argv) + '\n' + result.stdout + result.stderr)
    return result.stdout + result.stderr


def jobs():
    result = []
    for makefile in BUILD.rglob('build.make'):
        flags = makefile.with_name('flags.make')
        if not flags.exists():
            continue
        variables = dict(re.findall(r'^(\w+) = (.*)$', flags.read_text(), re.M))
        for line in makefile.read_text().splitlines():
            if not line.startswith('\t') or ' -c ' not in line or 'aarch64-none-elf-g' not in line:
                continue
            text = line.strip()
            cwd = BUILD
            if text.startswith('cd '):
                directory, text = text[3:].split(' && ', 1)
                cwd = Path(native(shlex.split(directory)[0]))
            text = re.sub(r'\$\((\w+)\)', lambda m: variables[m[1]], text)
            argv = [native(x) for x in shlex.split(text)]
            if not Path(argv[0]).name in ('aarch64-none-elf-gcc.exe', 'aarch64-none-elf-g++.exe'):
                raise RuntimeError('Unexpected compiler command')
            argv.insert(1, '-I' + GLM.as_posix())
            argv.insert(1, '-mtp=soft')
            argv.insert(1, '-DMOONLIGHT_STABLE_BUILD=1')
            argv.insert(1, '-mtls-dialect=trad')
            argv.insert(1, '-ftls-model=local-exec')
            source = Path(argv[argv.index('-c') + 1])
            output = cwd / argv[argv.index('-o') + 1]
            old_tls_object = output.exists() and not TLS_ABI_STAMP.exists() and has_tls_relocations(output)
            if output.exists() and not old_tls_object and output.stat().st_mtime >= max(source.stat().st_mtime, (ROOT / "app/src/utils/Settings.hpp").stat().st_mtime if "/app/" in source.as_posix() else 0):
                continue
            output.parent.mkdir(parents=True, exist_ok=True)
            result.append((argv, cwd, source.name))
    return result


def main():
    if not (GLM / 'glm/vec2.hpp').exists():
        raise RuntimeError('Missing GLM headers')
    if not (BUILD / 'CMakeFiles/Moonlight.dir/link.txt').exists():
        raise RuntimeError('Configure build/switch-local first')
    work = jobs()
    print('Native compile jobs:', len(work), flush=True)
    with LOG.open('w', encoding='utf-8') as log:
        with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
            pending = {pool.submit(run, args, cwd): name for args, cwd, name in work}
            for n, future in enumerate(concurrent.futures.as_completed(pending), 1):
                try:
                    log.write(future.result())
                    log.flush()
                except Exception as error:
                    log.write(str(error)); log.flush()
                    for other in pending:
                        other.cancel()
                    raise
                print(f'[{n}/{len(work)}] {pending[future]}', flush=True)

        for link in BUILD.rglob('link.txt'):
            lines = link.read_text().splitlines()
            if not lines:
                continue
            argv = [native(x) for x in shlex.split(lines[0])]
            if 'gcc-ar' not in argv[0]:
                continue
            argv[1] = 'rcs'
            cwd = link.parent.parent.parent
            log.write(run(argv, cwd));log.flush()
            print('Archive:', argv[2], flush=True)

        link = BUILD / 'CMakeFiles/Moonlight.dir/link.txt'
        argv = [native(x) for x in shlex.split(link.read_text())]
        argv[1:1] = ['-Wl,--wrap=setsockopt', '-Wl,--wrap=av_nvtegra_dfs_init', '-Wl,--wrap=av_nvtegra_dfs_update', '-Wl,--wrap=dkQueueAcquireImage', '-Wl,--wrap=dkQueuePresentImage', '-Wl,--wrap=dkSwapchainCreate']
        dav1d_directory = ROOT.parent / '.local-tools/dav1d-build/src'
        if (dav1d_directory / 'libdav1d.a').exists():
            argv.insert(1, '-L' + dav1d_directory.as_posix())
        log.write(run(argv, BUILD));log.flush()
        print('ELF linked', flush=True)
        log.write(run([sys.executable, str(ROOT / 'scripts/test_switch_tls_abi.py')], ROOT))
        log.flush()
        print('Switch TLS ABI verified', flush=True)
        TLS_ABI_STAMP.write_text('libnx software thread pointer ABI\n')

    destination = ROOT / 'build/dist'
    destination.mkdir(exist_ok=True)
    resources = ROOT / 'build/native-resources'
    shutil.copytree(ROOT / 'resources', resources, dirs_exist_ok=True,
                    ignore=lambda path, names: ['font'] if Path(path) == ROOT / 'resources' else [])
    nacp = destination / 'Moonlight-LowLatency.nacp'
    run([str(SDK/'tools/bin/nacptool.exe'), '--create', 'Moonlight Stable LL27', 'XITRIX / custom', '1.6.0-LL27', str(nacp)], BUILD)
    nro = destination / 'Moonlight-LowLatency.nro'
    run([str(SDK/'tools/bin/elf2nro.exe'), str(BUILD/'Moonlight.elf'), str(nro),
         '--icon='+str(ROOT/'resources/img/moonlight_icon.jpg'),
         '--nacp='+str(nacp), '--romfsdir='+str(resources)], BUILD)
    print('NRO:', nro, nro.stat().st_size, 'bytes', flush=True)


if __name__ == '__main__':
    main()
