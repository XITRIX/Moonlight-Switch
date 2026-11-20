"""Use libounce's OS-seeded ChaCha generator for Expat's hash salt."""

from pathlib import Path

p = Path("build/deps/src/expat/lib/xmlparse.c")
source = p.read_text()
marker = "#if ! defined(HAVE_GETRANDOM) && ! defined(HAVE_SYSCALL_GETRANDOM)"
adapter = """#if defined(__SWITCH2__)
#  include <switch/kernel/random.h>
#  define HAVE_ARC4RANDOM_BUF 1
#  define arc4random_buf randomGet
#endif

"""
if adapter not in source:
    assert source.count(marker) == 1
    p.write_text(source.replace(marker, adapter + marker))
