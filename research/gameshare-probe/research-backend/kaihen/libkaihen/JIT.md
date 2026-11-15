# JIT for game-role homebrew

Native Switch 2 homebrew can use libounce's libnx-compatible `Jit` API by
linking `-lkaihen` from the Switch 2 devkit. That names the provider object,
`kaihen-jit.o` (`make` in this directory builds it in `build/`). It is an
object rather than an archive member because libounce references its hooks
weakly. Applications keep using
`<switch/kernel/jit.h>`, `jitCreate`, `jitGetRwAddr`,
`jitTransitionToWritable`, `jitTransitionToExecutable`, and `jitClose`.

## Persistent alias backend

The game's policy does not allow the code mapping syscalls. RO has those
syscalls and receives the game's process handle from its initialized
`ldr:ro` session. hbloader passes that session to each NRO as an override.

With the kaiHEN provider linked, `jitCreate` allocates locked backing, a
stable game RX address, and a separate game RW staging buffer. RO maps the
backing as RX in the game (private command 65010), then retains its own RW
alias of that RX range (65013). `jitGetRwAddr` returns staging, which is not a
direct physical alias of RX. The public `Jit` struct layout and function
signatures remain those of libnx.

`jitTransitionToWritable` changes logical publication state; it does not
remove the RX mapping. `jitTransitionToExecutable` sends the entire staging
buffer to RO (65014). RO copies it through its alias and cleans the written
data cache. On success, libounce invalidates the calling thread's RX
instruction range. For sparse updates, the optional kaiHEN API
`kaihenJitPublishRanges` coalesces adjacent ranges, packs only their staged
bytes and sends them in one request (65015), then invalidates those ranges
on the calling thread.

RO unmaps its alias before the game RX mapping on `jitClose` (65011). If an
NRO returns with mappings still live, hbloader sends 65012 before reusing the
NRO heap. The staging buffer uses additional physical memory about equal to
the locked backing size.

RO records up to 512 live JIT mappings per initialized `ldr:ro` client
session. This is the helper's record limit; kernel mapping and available
memory limits can be lower. The 512-slot console probe is `jit-capacity.nro`.
It passed the host test and a console test.

Do not modify a block another thread is executing. A thread entering newly
published code must synchronize with the publisher and invalidate its own RX
instruction range. The publisher's invalidation does not synchronize every
core. Stop readers before `jitClose`.
