"""Run every host test and retain outputs for each release.

These tests do not replace playback on actual Switch hardware.
"""
from pathlib import Path
import concurrent.futures
import json
import os
import shutil
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
compiler = ROOT.parent / '.local-tools/w64devkit/bin'
if compiler.exists():
    os.environ['PATH'] = str(compiler) + os.pathsep + os.environ['PATH']
assert shutil.which('g++') and shutil.which('gcc'), 'Host compilers missing'

def run(path):
    started = time.monotonic()
    command = [sys.executable, str(path)]
    if path.name == 'test_switch_network.py': command += ['--cc', 'gcc']
    if path.name == 'test_frame_queue.py': command += ['--cxx', 'g++']
    try:
        result = subprocess.run(command, cwd=ROOT,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                encoding='utf-8', errors='replace', timeout=180)
    except (subprocess.TimeoutExpired, OSError) as error:
        return {'test': path.name, 'passed': False,
                'seconds': round(time.monotonic() - started, 3),
                'output': str(error)}
    return {'test': path.name, 'passed': result.returncode == 0,
            'seconds': round(time.monotonic() - started, 3),
            'output': result.stdout}

tests = sorted(p for p in Path(__file__).parent.glob('test*.py')
               if p.name != Path(__file__).name)
with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
    results = list(pool.map(run, tests))
report = ROOT / 'build/test-results/host-tests.json'
report.parent.mkdir(parents=True, exist_ok=True)
report.write_text(json.dumps({'hardware_tested': False, 'results': results},
                            indent=2), encoding='utf-8')
for result in results:
    print(('PASS' if result['passed'] else 'FAIL') + ': ' + result['test'])
    if not result['passed']:
        print(result['output'])
print(f'{sum(r["passed"] for r in results)}/{len(results)} suites passed; {report}')
sys.exit(0 if all(r['passed'] for r in results) else 1)
