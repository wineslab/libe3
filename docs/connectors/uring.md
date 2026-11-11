# The io_uring link

`E3LinkLayer::URING` drives the POSIX link's sockets through Linux io_uring. It puts the same
bytes on the same sockets as `E3LinkLayer::POSIX`: a RAN of one link serves a dApp of the other,
over IPC, TCP and SCTP alike. What changes is how a thread waits for those bytes and writes them,
and with that, the latency of the E3 hop and the CPU it costs.

## What a user can do with it

| Today, with `--link posix` | With `--link uring` |
|---|---|
| A received PDU costs two system calls (`poll`, then `recv`) and a thread wakeup | One `io_uring_enter` that re-arms the receive and waits; the wakeup stays |
| The receiving thread always sleeps between PDUs | It can poll the completion queue instead of sleeping (`LIBE3_URING_BUSY_POLL_US`), which takes the thread wakeup out of the hop: the wire hop fell from 7 to 2 µs on the x86 server measured below (from 20 to 3.5 to 6 µs on the Arm virtual machine), and the round trip by 12 to 48% (27 to 29% on Arm), for 1.1 to 1.6 times the CPU (2.4 times on Arm). With a kernel submission thread as well (`LIBE3_URING_SQPOLL`), a wire hop took 1.7 µs or less and a send 2.3 to 10 µs |
| A RAN sending to N dApps makes N `sendmsg` calls one after the other; a dApp that stopped reading holds the others back for up to the 500 ms send timeout | The N writes are submitted together; the others have their PDU at once, and only the call waits for the stalled one |
| Nothing to set up | Linux 5.11 or later, a libe3 built with liburing, and a process allowed to use io_uring (see [Where it runs](#where-it-runs)) |

Nothing else changes: the same `E3Config` fields, endpoints, ports, `PosixOptions`, C API,
Python binding and threads. To use it, set `link_layer = E3LinkLayer::URING` (`--link uring` in
`example_simple_agent`, `example_simple_dapp` and `test_bench_full_loop_latency`, `2` in the C
API's `e3_config_t.link_layer`, `E3LinkLayer_URING` in Python).

Which link to pick, from the measurements at the end of this page (two machines, an x86-64 server
seen through a 12-vCPU virtual machine and a 6-vCPU Arm virtual machine; interleaved rounds of 3000
round trips per cell; IPC, TCP and SCTP; three encodings). The round trips below are ASN.1 medians on
the x86 server, with the Arm virtual machine's in brackets:

- **Lowest latency, with cores to give it:** `uring` with `LIBE3_URING_SQPOLL=1` and
  `LIBE3_URING_BUSY_POLL_US` above the gap between PDUs. It was the fastest on every transport and
  encoding on both machines: 20.7 µs on IPC, 27.3 on TCP and 33.1 on SCTP, against POSIX's 50.4, 47.3
  and 48.6 (59, 42 and 32% less) [19.4, 19.6 and 34.3 against 110.5, 115.7 and 122.0], and 12.4 µs on
  IPC with Protobuf against 29.9. It costs 1.4 to 2.0 times POSIX's CPU [2.6] and a spinning kernel
  thread per connector. Its p99 was 27 to 51% below POSIX's on the x86 server (52 to 68 µs against 92 to
  111) but above it on the Arm virtual machine (218 to 305 against 165 to 180), where the spinning
  threads shared six virtual CPUs with the rest. What the gain is made of differs too: on the x86
  server it is the connector's own rows (the wire hop and the send call, below); on the Arm virtual
  machine it was mostly that no thread in the loop slept any more (the queuing rows fall from 19 µs to
  1.4 µs), and that holds only while PDUs come faster than libe3's queue spins (tens of microseconds).
- **Lower latency on any machine:** `uring` with `LIBE3_URING_BUSY_POLL_US` alone: 48, 27 and 12% below
  POSIX on IPC, TCP and SCTP [27 to 29%], at 1.1 to 1.6 times the CPU [2.4], and a p99 15 to 49% lower on
  the x86 server.
- **Otherwise POSIX; default io_uring is no better or worse than it, within the noise.** On the x86
  server io_uring without busy polling was 12% below POSIX on IPC, 5% above on TCP and 12% above on
  SCTP, and the rounds of the two overlap on all three [0 to 4% above on Arm]. The wire hop is shorter on
  IPC (4.8 against 7.2 µs) and the same on TCP, the send call costs 0.8 to 1.1 µs more, and the wakeups
  that dominate both are the same. POSIX needs nothing from the kernel or the container.
- **`LIBE3_URING_SQPOLL` without busy polling is not a general choice:** the sending thread sleeps
  until the kernel thread has done the write. On the x86 server it gave 35.6 µs on IPC (29% below
  POSIX) but 48.3 on TCP (the same) and 56.2 on SCTP (16% above), at 1.5 to 1.9 times the CPU; on the
  Arm virtual machine it was 31 to 45% above POSIX, at 3.5 to 3.8 times the CPU.
- ZeroMQ was the slowest: 1.8 to 2.1 times POSIX's round trip on the x86 server, twice on Arm.

## Architecture

```
            receive()/recv_setup_request()          send()/send_to()/send_response()
            the thread holding reader_mutex_        the thread holding tx_mutex_
                        │                                         │
                ┌───────▼────────┐                        ┌───────▼────────┐
                │  receive ring  │  accept, one recv per  │   send ring    │  the writes of one call,
                │   (UringRing)  │  connection, a wake    │   (UringRing)  │  submitted together
                └───────┬────────┘  eventfd read          └───────┬────────┘
                        │                                         │
                        └────────────── sockets ──────────────────┘
                     socket_link: endpoint, options, admission, framing
                     (shared with PosixE3Connector: one wire)
```

| File | What it holds |
|---|---|
| `src/connector/socket_link.{hpp,cpp}` | The endpoint parsing, the socket options, the admission rules and the framing, shared by the POSIX and the io_uring connector. Moved out of `posix_connector.cpp` without a change of behaviour; the POSIX tests are the proof |
| `src/connector/uring_ring.{hpp,cpp}` | One ring. The only file of libe3 that includes `<liburing.h>`; completions come out as a plain `RingCompletion` |
| `src/connector/uring_connector.{hpp,cpp}` | `UringE3Connector`: the connections, the receive and send paths, the ring options |

### Decisions, and what was not done

**1. A connector of its own, on the POSIX wire.** The alternatives were a mode inside
`PosixE3Connector` (the benchmark could no longer put both in one build side by side, and that
file is already a thousand lines) and a new wire (it would split the peers into two camps that
cannot talk). Instead the bytes are the POSIX link's, which since #153 frames every transport the
same way in both directions, so mixed pairs work. The code that decides the bytes lives once, in
`socket_link`, so the two links cannot drift apart; the mixed pairs are tested (below).

**2. The facade gains one enumerator.** `E3LinkLayer::URING = 2`, its string `uring`, the C API's
range, and the factory case. No new `E3Config` field and no new `E3Connector` method: the
`PosixOptions` apply as they are, because the sockets are the same. The ring options are tuning
for a deployment, not protocol, so they come from the environment (`LIBE3_URING_*`, read once
when the connector is made), the way latrec takes `LATREC_ENTRIES_LOG2`; they are not ABI and can
change without a version bump. A build without liburing keeps the enumerator and fails
`create_connector` with a message, exactly as a build without ZeroMQ does for `ZMQ`; there is no
silent fallback to POSIX, because a deployment that asked for io_uring and silently got something
else would be measured wrong.

**3. The calling threads drive the rings; there is no I/O thread.** A dedicated I/O thread would
let `send()` return on enqueue, but it adds a thread hop to every PDU in both directions, which
costs a wakeup (about 20 µs on the machine below, a few microseconds on bare metal) and is
exactly what the link is meant to remove. So the thread that calls `receive()` waits on the
receive ring, and the thread that calls `send()` submits its writes and waits for their
completions. `send()` therefore returns when the kernel has the bytes, as with POSIX, and
`SEND_DONE` means the same on both links: the latrec rows compare one to one.

**4. Two rings, one per direction.** A RAN's inbound thread waits on the receive ring for up to
half a second; the outbound thread must not wait behind it to submit a write. One ring each,
guarded by the mutex of its direction, keeps the two independent. With a kernel submission
thread the two rings share one (`IORING_SETUP_ATTACH_WQ`).

**5. One receive armed per connection, re-armed with the wait.** The receive ring holds a receive
on every connection (and the accept, on a RAN). When `receive()` needs a PDU it re-arms what
completed and waits in the same `io_uring_enter`, so a PDU costs one system call where POSIX needs
two. A TCP receive may bring several PDUs; they are queued and handed out one per `receive()`, in
the order they came, with the close of a connection queued behind its last PDU so that
`take_closed_peers()` reports it only after that PDU was taken (the contract of `E3Connector`).
Multishot receives with provided buffers would also save the re-arm, but the wait stays one
system call per PDU when the thread sleeps, and they need kernel 6.0; they are deferred (below).

**6. The kernel's default way of running completions, not `SINGLE_ISSUER`/`DEFER_TASKRUN`.** The
facade lets more than one thread read (a dApp's setup thread reads the setup response, then the
inbound thread reads the rest; a RAN's setup thread reads when nothing calls `receive()`) and more
than one thread send (setup and outbound). `IORING_SETUP_SINGLE_ISSUER` forbids that, and
`DEFER_TASKRUN` needs it. In the default mode the kernel finishes a completed receive on the
thread that armed it, and makes that thread enter the kernel with `TIF_NOTIFY_SIGNAL`, the
mechanism of a signal. Two consequences, both handled or written down:

- A blocking call of that thread with a timeout returns `EINTR` when one of its receives
  completes. The tests found it: a test thread that had read for the RAN then called `connect()`
  (whose socket has a send timeout), and the connect failed. So a thread that reads only in
  passing (the setup thread standing in for an idle `receive()`) takes back everything it armed
  before it returns, and only the steady reader keeps receives armed. Within an `E3Agent` that is
  the inbound thread, so **a Service Model's control handler, which runs on the RAN's inbound
  thread, can see `EINTR` from a socket call that has `SO_RCVTIMEO` or `SO_SNDTIMEO`, or from
  `epoll_wait`, while other dApps' receives are armed.** Calls without a timeout restart by
  themselves. A dApp's handlers run while nothing of the dApp's is armed. With a kernel
  submission thread none of this happens: that thread arms and runs everything.
- Per-thread rings with `SINGLE_ISSUER | DEFER_TASKRUN` would remove both the interruption and the
  wakeups it costs, at the price of a ring per thread, the registered files kept per ring, and a
  hand-over between the setup and the inbound thread. Deferred, with its cost, below.

**7. Busy polling and the kernel submission thread are opt-in.** Both trade CPU for latency, and
neither is free to switch on in a shared machine:

- `LIBE3_URING_BUSY_POLL_US=N`: a thread waiting for a completion reads the completion queue for up
  to N µs before it sleeps. Without a submission thread the kernel still needs the waiting thread
  to finish the receive, and interrupts it to do so (an inter-processor interrupt, a few hundred
  nanoseconds) instead of waking it from sleep (the scheduler, a C-state exit, and in a VM a halted
  virtual CPU: about 20 µs below). It keeps one core busy per receiving thread for the gap between
  PDUs. A receiving thread that busy-polls must not share its core with the sending thread:
  `io_thread_affinity` pins both I/O threads of an agent to one core, so do not combine the two.
- `LIBE3_URING_SQPOLL=1`: a kernel thread takes the submissions and finishes the completions, so
  neither direction makes a system call, and no user thread is interrupted. It spins for a second
  after the last submission (`sq_thread_idle`), on the core of `LIBE3_URING_SQPOLL_CPU` if set.
  With busy polling too it gave a wire hop under a microsecond and the fastest round trip measured;
  without busy polling it is the slowest variant, because the sending thread then sleeps until the
  kernel thread has written. It needs cores of its own: on a busier machine its spinning threads
  competed with libe3's others and it fell behind busy polling alone.

**8. Registered files on, registered buffers off, by default; neither measurably matters.** Each
connection takes a slot in both rings' file tables (`IORING_REGISTER_FILES`, sparse), so an operation skips the descriptor
lookup and its reference count; the first 256 connections get a slot and later ones use the plain
descriptor. Registered buffers (`READ_FIXED` into a buffer registered per connection) are offered
for the measurement but left off: a socket receive copies the bytes either way, so all they save is
the user-pointer checks, and they pin 64 to 128 KiB per connection against `RLIMIT_MEMLOCK`. SCTP
never uses them, as it needs `recvmsg` for the payload protocol id. Each falls back to the plain
way when the kernel refuses it, and `UringE3Connector::settings()` says what is in effect. Measured,
neither registered files nor registered buffers moved the round trip beyond the spread of the
rounds: the descriptor lookup they save is tens of nanoseconds in a hop of microseconds. Files stay
on because they cost nothing on the data path.

**9. A broadcast is one batch.** `send_to(0, …)` prepares one write per connection and submits
them together. Writes that complete at once (the usual case) cost one system call for all of them.
A write still pending after `SEND_TIMEOUT_MS` (500 ms, as POSIX) is cancelled, its connection is
shut, and the reader then reports it closed, which is what POSIX does after `SO_SNDTIMEO`. A
short TCP write is completed by submitting the rest. The call returns only when the kernel holds no
reference to the caller's buffer.

**10. Shutdown wakes the reader through an eventfd.** `shutdown()` is the one call made from
another thread while a reader waits. It shuts the sockets, as POSIX does, and also writes an
eventfd whose read is armed on the receive ring, so a reader that waits on nothing else (a RAN with
no dApp) returns at once. `dispose()` takes back everything still armed before it frees the
buffers, then tears the rings down; it can be called twice.

**11. SCTP bounds its retransmission timeout, on both socket links.** Repeating the tests found a
send to a dApp over SCTP that stalled past the 500 ms send timeout, so the RAN dropped the dApp. It
was not io_uring: a burst of 2000 PDUs of up to 64 KiB from the RAN dropped the dApp in 8 of 100
runs on POSIX and 7 of 100 on io_uring, and never over TCP. A burst closes the receiver's window,
and the sender then waits on SCTP's retransmission timer, whose smallest value in Linux is 1 s
(`net.sctp.rto_min`). With the minimum at 20 ms the same runs never dropped a dApp and the
slowest send took 25 ms. So the shared socket setup gives every association a timeout of 120 ms at
first and 20 to 250 ms after (`SCTP_RTOINFO`, the per-socket form of the sysctl, the same idea as
the 120 and 500 ms that telecom SCTP stacks configure); 250 ms is half the send timeout, so one
backed-off wait cannot reach it. With it, the same 100 runs at the kernel's default sysctl dropped no
dApp on either link (slowest send 23 to 25 ms). E3 runs between neighbours (the same host, or one switch apart),
where 20 ms is still far above any round trip. It changes the POSIX link's SCTP timers as well,
which keeps the two links the same; `test_socket_link` reads the values back from the kernel.

<a id="where-it-runs"></a>
## Where it runs

| Requirement | Why | What happens otherwise |
|---|---|---|
| Linux 5.11 or later | `IORING_FEAT_EXT_ARG` (waits with a timeout without a ring entry) and `IORING_FEAT_FAST_POLL` | `create_connector` returns null and logs "Linux 5.11 or later is needed" |
| liburing 2.0 or later at build time (`liburing-dev`, `liburing-devel`) | the ring API | the link is not built; `--link uring` fails with "built without it" |
| io_uring allowed for the process | Docker's default seccomp profile refuses `io_uring_setup`, and `kernel.io_uring_disabled` set to 1 (allowed for one group) or 2 (nobody) refuses it on the host; Red Hat Enterprise Linux 9 and so OpenShift's nodes ship 2, which is why the x86 measurements below were taken in a virtual machine on such a cluster | `create_connector` returns null and logs which of the two to change: `--security-opt seccomp=unconfined` or a profile that allows `io_uring_setup`, `io_uring_enter` and `io_uring_register`; or the sysctl |
| `RLIMIT_MEMLOCK` | only for registered buffers | the buffer falls back to a plain one, with a warning |

The tests that need io_uring say `URING NOT RUN` and exit 77 (CTest's "skipped") where it is
refused, unless `LIBE3_REQUIRE_URING` is set: then they fail, so that CI cannot pass by skipping.

## Ring options

Read from the environment when the connector is made (`E3Interface::init()`, or each restart of a
dApp). A value out of range is ignored with a warning.

| Variable | Default | Range | What it does |
|---|---|---|---|
| `LIBE3_URING_DEPTH` | 64 | 4 to 4096 | Submission queue entries of each ring; the completion queue has four times as many. A broadcast to more connections than this is submitted in pieces |
| `LIBE3_URING_FIXED_FILES` | 1 | 0, 1 | Registered files (decision 8) |
| `LIBE3_URING_FIXED_BUFFERS` | 0 | 0, 1 | Registered receive buffers on TCP and IPC (decision 8) |
| `LIBE3_URING_BUSY_POLL_US` | 0 | 0 to 10000000 | Poll the completion queue this long before sleeping (decision 7) |
| `LIBE3_URING_SQPOLL` | 0 | 0, 1 | A kernel submission thread per connector (decision 7) |
| `LIBE3_URING_SQPOLL_CPU` | -1 | -1 to 4095 | The CPU of that thread; -1 lets the scheduler place it |

## For a real-time deployment

- Give each busy-polling receiving thread a core of its own, isolated from the scheduler
  (`isolcpus`, `nohz_full`, `rcu_nocbs`, as `libe3-paper/scripts/run_baseline.sh` does), and set the
  budget above the gap between PDUs (an indication per slot at 30 kHz is 500 µs). A budget shorter
  than the gap still sleeps, only later.
- Do not pin a busy-polling agent with `io_thread_affinity`: it pins the inbound and the outbound
  thread to the same core, and the spinning one starves the other.
- With `LIBE3_URING_SQPOLL`, pin the kernel thread (`LIBE3_URING_SQPOLL_CPU`) to an isolated core
  as well; one per connector, so two for a RAN and a dApp in one process.
- In a container: allow io_uring in the seccomp profile. In Kubernetes and OpenShift that is a
  `seccompProfile` of type `Localhost` (or `Unconfined`), and on a node whose kernel ships
  `kernel.io_uring_disabled=2`, a node-level sysctl change.
- An SM control handler that makes socket calls with a timeout on the RAN's inbound thread should
  retry on `EINTR` (decision 6), or the deployment should use the kernel submission thread.

## Tests

Written before the code, in the order of the issue's plan, and run on Linux 6.8 on two machines (an
x86-64 server's virtual machine and an Arm virtual machine) and, early on, on 7.0 in a Docker
container with seccomp unconfined: the whole suite with io_uring and SCTP required (81 of 81 on
x86-64), `build_libe3` from a clean tree (62 of 62), the io_uring and link-matrix tests fifteen
times over, the connector tests under AddressSanitizer with UndefinedBehaviorSanitizer (no report)
and under ThreadSanitizer (every report is inside libzmq's own threads), the example RAN and dApp as
two processes on every pair of links and transport (14 of 14), and a real gNB (below).

- **The wire, against itself and against POSIX.** The POSIX connector and pair tests run again,
  unchanged, with `--link uring`, and with a RAN of one link and a dApp of the other
  (`tests/link_under_test.hpp`; CTest names `*_uring`, `*_uring_ran_posix_dapp`,
  `*_posix_ran_uring_dapp`): `test_role_aware_connector_posix` (framing on every transport, the
  old layout refused, `send_to` to one peer, the peer list, the cap, IPv6),
  `test_dapp_bound_to_connection_posix`, `test_dapp_link_closed_posix`, and the integration tests
  `test_role_pair_posix` and `test_role_pair_sctp` (payload protocol id and streams read off the
  association by a proxy). These extra runs set `LATREC_ENTRIES_LOG2=12`: in a latrec build every
  agent thread leaves a pre-faulted ring in the build tree, 8 MiB by default, and four runs of each
  test would otherwise fill a small disk when the suite is repeated.
- **Only io_uring** (`test_uring_connector`): each ring variant (plain, registered files, registered
  buffers, submission thread, busy polling, all together) carries a setup and both directions on
  IPC, TCP and SCTP and is the variant in effect; a broadcast to more connections than ring entries;
  a stalled peer does not delay the others, which have their PDU long before the send gives up on
  it; a reset peer is dropped alone; 2000 PDUs of every size up to 65536 octets, both ways, on
  every transport; the size limit; the receive timeout; `shutdown()` waking a waiting `receive()`.
- **The agent over it**: `test_dapp_ran_loss`, `test_dapp_reconnect`,
  `test_dapp_refused_setup_reconnect`, `test_multi_peer_dispatch` (including the slow-peer case) and
  `test_latrec_stages` (every PDU sent is stamped `SEND_DONE` after `ENCODE_E3AP_DONE`) each gained
  io_uring cases.
- **The build**: `test_link_availability` runs in every configuration and checks that the link is
  there exactly when built, that a build without it refuses `--link uring`, and that link layer 2
  of the C API is the io_uring link.
- **The framing** shared by both links, as pure functions (`test_socket_link`): split frames, two
  frames in one read, a length of zero or above the limit, the endpoint parsing; and the SCTP
  retransmission timeout of decision 11, read back from the kernel.
- **Ports.** The tests take their ports from `free_ports()` (`tests/link_under_test.hpp`): below the
  range the kernel uses for the source ports of outgoing connections, bound once to see that they are
  free, and claimed with a lock on a file that the process holds until it exits, so that no two tests
  take the same port however they order asking and listening. Ports from inside the source-port range
  collided with a source port still in TIME_WAIT (two runs in thirty of `test_uring_connector`), and
  ports derived from the pid with a different base in each test overlapped each other (eight runs in
  twelve of the connector tests at `-j3`).
- **A real gNB** (not in CTest: it needs an OAI build, about 10 GB): the OAI gNB with its E3 agent
  (`--E3Configuration.link uring`) and one nrUE on the RF simulator, and the dApp library's spectrum
  dApp over the other end, on IPC, TCP and SCTP with the RAN and the dApp each on POSIX or io_uring: 12
  of 12 pairs run, 16,000 to 18,600 sensing slots handled in 15 s and 99.7 to 99.9% of them with their
  sensing ranges, no assertion in the gNB. Two things were found there that are not about io_uring:
  an IPC endpoint is created with the permissions of the RAN's user, so a dApp of another user cannot
  connect to it (the same on POSIX), and the dApp library's spectrum dApp asks the L1-KPM function for a
  control identifier that it does not have, which libe3 now refuses (#96); the run used an empty list.
- **The comparison tool** (`test_bench_matrix`): the numbers it reads from the benchmark's table, and
  its verdict, on canned output.

Issue #140 proposed a portable layer under the connector (a completion loop fed by a fake ring) so
that the completion handling could run on macOS. It was not built: since #153 the library targets
Linux only, the framing is tested as pure functions instead, and the completion handling (a reset
peer, a cancelled write, short writes, a full queue) with real sockets, where the kernel's own
behaviour is what is being tested.

CI: the unit and integration jobs set `LIBE3_REQUIRE_URING`; the end-to-end matrix gained
`uring` (and SCTP for POSIX and io_uring); a build with `-DLIBE3_ENABLE_URING=OFF` runs the suite;
the full-loop stage runs the comparison below on its runner and puts it in the CI report.

## Measurements

**How.** `tools/bench_matrix.py` runs `test_bench_full_loop_latency` (a RAN and a dApp in one
process, the Simple Service Model in ping-pong: one indication in flight, the next emitted when the
control comes back) for each link variant and transport, in rounds that interleave the cells so a
drift of the machine hits them all alike, and reads back the 18-phase latrec table of each run. The
round trip is the primary number; the send call (A6, A15), the wire hop (`SEND_DONE` to `RECV`) and
the CPU of the process per round trip, both ends together, explain it. `send()` returns when the
kernel has the bytes on both socket links, so their rows compare one to one. The CI full-loop stage
runs the same comparison (ZeroMQ, POSIX, io_uring, and io_uring with busy polling, with and without
the submission thread, on IPC, TCP and SCTP, three rounds) and puts the table in the CI report.

A wire hop can come out below zero. `SEND_DONE` is stamped after `send()` has returned, and a receiver
that polls can have the PDU, and stamp `RECV`, before that: SCTP over loopback delivers inside the
sender's system call, and io_uring completes a send on a kernel thread. The benchmark pairs the dApp's
inbound records with the RAN's outbound ones by position, and checks each pair for the right order,
allowing the stamps 5 µs of skew (a pair that is off by one round trip is off by 12 µs or more). Without
that allowance, on the x86 server, SCTP runs with busy polling were refused as misaligned in about one
run in ten (12 of 110; nothing was lost, the counts of PDUs out and in agreed); the tables below show the
hop as it is, below zero where it is. The Arm tables were taken with the stricter check, so their SCTP wire
figures may sit a little high.

**Where.** Two machines, to see what depends on the machine. The tables in the first subsection are
the reference; the second is the earlier campaign.

### On the x86 server

An Intel Xeon Gold 6240R (Cascade Lake, 2.4 GHz) server of a Kubernetes cluster, seen through a KubeVirt
virtual machine of 12 virtual CPUs and 24 GB: Ubuntu 24.04, Linux 6.8, liburing 2.5, Release build,
latrec on, nothing pinned or isolated, other workloads on the host (it was lightly loaded). It had to be
a virtual machine: the host kernels of that cluster set `kernel.io_uring_disabled=2` (the RHEL default),
which hides io_uring from every container on them. Waking a sleeping thread costs a few microseconds
here, less than on the Arm virtual machine below, so the wakeups weigh less. Not bare metal and not
isolated cores: the p99 and the spread of the rounds show it.

**Round trip, ASN.1, nine rounds of 3000 round trips each** (median over the rounds of each round's
statistic, the range of the rounds in brackets, microseconds; CPU is the process, both ends, per round
trip; `busy` is `LIBE3_URING_BUSY_POLL_US=1000`, `sqpoll` is `LIBE3_URING_SQPOLL=1`, `plain` is
registered files off, `fixedbuf` registered buffers on):

| Transport | Link | Round trip p50 | p99 | Wire RAN to dApp p50 | Wire dApp to RAN p50 | Send indication p50 | Send control p50 | CPU |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| IPC | zmq | 88.8 [65.1 to 92.8] | 158.6 | 23.8 | 23.6 | 3.6 | 3.6 | 186 |
| IPC | posix | 50.4 [41.4 to 53.7] | 105.7 | 7.2 | 7.8 | 4.0 | 3.7 | 138 |
| IPC | uring | 44.1 [30.2 to 49.8] | 94.9 | 4.8 | 5.1 | 4.8 | 4.7 | 128 |
| IPC | uring, plain | 41.3 [31.6 to 50.5] | 80.4 | 4.8 | 4.9 | 4.7 | 4.6 | 121 |
| IPC | uring, fixedbuf | 40.5 [38.5 to 50.7] | 80.5 | 4.9 | 4.9 | 4.8 | 4.5 | 122 |
| IPC | uring, busy | 26.2 [23.2 to 29.2] | 53.8 | 2.0 | 2.2 | 4.0 | 4.0 | 149 |
| IPC | uring, sqpoll | 35.6 [27.7 to 41.2] | 83.1 | 1.8 | 1.8 | 7.3 | 7.7 | 201 |
| IPC | **uring, sqpoll + busy** | **20.7** [18.2 to 22.6] | 51.6 | 1.6 | 1.7 | 2.4 | 2.3 | 194 |
| TCP | zmq | 98.9 [73.4 to 102.2] | 189.6 | 30.1 | 27.9 | 3.8 | 3.7 | 204 |
| TCP | posix | 47.3 [41.6 to 53.5] | 92.0 | 3.3 | 3.7 | 8.0 | 7.7 | 129 |
| TCP | uring | 49.6 [42.1 to 56.0] | 99.9 | 3.4 | 3.4 | 9.1 | 9.0 | 132 |
| TCP | uring, plain | 52.5 [44.1 to 56.6] | 114.5 | 3.4 | 3.6 | 9.2 | 9.1 | 142 |
| TCP | uring, fixedbuf | 53.1 [43.6 to 58.4] | 113.7 | 4.0 | 3.6 | 9.8 | 9.3 | 144 |
| TCP | uring, busy | 34.5 [26.4 to 36.6] | 66.9 | 0.8 | 1.0 | 8.1 | 7.8 | 183 |
| TCP | uring, sqpoll | 48.3 [34.8 to 54.2] | 96.6 | 1.0 | 1.1 | 11.1 | 11.0 | 233 |
| TCP | **uring, sqpoll + busy** | **27.3** [25.8 to 31.1] | 67.6 | 0.8 | 1.0 | 5.8 | 5.4 | 235 |
| SCTP | posix | 48.6 [33.8 to 55.1] | 110.7 | 0.2 | 0.7 | 13.2 | 12.7 | 139 |
| SCTP | uring | 54.2 [35.8 to 57.4] | 109.2 | 0.1 | 0.2 | 14.1 | 14.2 | 148 |
| SCTP | uring, plain | 57.0 [41.3 to 61.8] | 116.2 | 0.1 | 0.2 | 14.4 | 14.1 | 154 |
| SCTP | uring, fixedbuf | 52.2 [48.8 to 56.5] | 121.5 | 0.0 | 0.2 | 14.0 | 14.2 | 148 |
| SCTP | uring, busy | 43.0 [37.1 to 46.7] | 93.8 | -1.5 | -1.2 | 13.5 | 13.3 | 217 |
| SCTP | uring, sqpoll | 56.2 [40.0 to 61.8] | 110.9 | -0.6 | -0.4 | 15.6 | 15.2 | 261 |
| SCTP | **uring, sqpoll + busy** | **33.1** [29.1 to 40.6] | 66.5 | -0.8 | -0.7 | 9.9 | 9.7 | 271 |

**The same with JSON and Protobuf**, seven rounds of 3000, round trip p50 (the order of the links does
not change with the encoding):

| Transport | Encoding | posix | uring | uring, busy | uring, sqpoll + busy |
|---|---|---:|---:|---:|---:|
| IPC | JSON | 62.2 | 57.1 | 47.3 | **39.7** |
| IPC | Protobuf | 29.9 | 25.6 | 17.1 | **12.4** |
| TCP | JSON | 64.5 | 67.7 | 56.6 | **46.5** |
| TCP | Protobuf | 29.5 | 32.2 | 23.7 | **17.1** |
| SCTP | JSON | 67.3 | 70.5 | 64.0 | **50.2** |
| SCTP | Protobuf | 32.1 | 33.0 | 27.7 | **23.0** |

**Fastest: io_uring with the kernel submission thread and busy polling**, on every transport and
encoding, 59, 42 and 32% below POSIX with ASN.1. Next comes io_uring with busy polling alone. What each
number is made of, from the 18-phase table of one run each (IPC, ASN.1, 3000 round trips, p50 in
microseconds):

| Phase | posix | uring | uring, busy | uring, sqpoll + busy |
|---|---:|---:|---:|---:|
| Queuing, indication (4) | 0.89 | 0.66 | 0.55 | 0.55 |
| Send, indication (5) | 4.26 | 5.64 | 4.09 | 2.47 |
| Wire, RAN to dApp (6) | 7.02 | 4.49 | 2.03 | 1.59 |
| Decode, indication (7) | 1.54 | 1.53 | 1.46 | 1.23 |
| Queuing, control (13) | 0.69 | 0.72 | 0.58 | 0.54 |
| Send, control (14) | 4.56 | 4.46 | 4.07 | 2.38 |
| Wire, dApp to RAN (15) | 7.97 | 5.01 | 2.19 | 1.67 |
| Decode, control (16) | 1.64 | 1.51 | 1.55 | 1.29 |
| **Round trip** | **43.95** | **37.32** | **25.84** | **21.99** |
| CPU per round trip, both ends | 138.8 | 123.0 | 144.2 | 209.5 |
| Voluntary context switches per round trip | 3.50 | 3.41 | 1.09 | 1.11 |

- **The wire hop and the send call are where the connector acts.** io_uring's re-armed receive makes
  the hop 2.5 µs shorter than POSIX's `poll` and `recv` and its send call 1.4 µs longer (the submission
  and the wait for the completion). Busy polling takes the receiver's wakeup out of the hop (7.0 to 2.0
  µs, and 3.5 voluntary context switches per round trip fall to 1.1). The kernel submission thread makes
  the send a write to shared memory (4.1 to 2.5 µs) and the hop 1.6 µs.
- **No second effect from the queues.** The queuing rows are 0.5 to 0.9 µs at the median in every
  configuration here, because a thread wake costs a few microseconds, not twenty, and libe3's queue spins
  before it sleeps. On the Arm virtual machine below the same rows cost 19 µs and fell to 1.4 µs in the
  fastest configuration, which was most of its gain; here it is not.
- **SCTP's wire hop is about zero** and its send call is longer (13 µs against 8 µs for TCP and 4 µs for
  IPC): over loopback the kernel delivers the packet inside the sender's system call. For SCTP the send
  and the wire hop are to be read together, and a hop below zero is the receiver having the PDU before the
  sender has stamped its return.
- **What does not matter.** Registered files and registered buffers made no difference beyond the spread
  of the rounds (the `plain` and `fixedbuf` rows).
- **What it costs.** Busy polling 1.1 to 1.6 times POSIX's CPU, the submission thread with busy polling
  1.4 to 2.0 times, the submission thread alone 1.5 to 1.9 times. The ranges of the rounds are wide
  (a tenth to a third of the median), as expected of a virtual machine on a shared host; where two links'
  ranges overlap the tool says the order is within the noise rather than naming a winner.

### On the Arm virtual machine

A Linux virtual machine on an Apple Silicon Mac (Lima, Apple's Virtualization framework):
Ubuntu 24.04, Linux 6.8, 6 virtual CPUs, 4 GB, liburing 2.5, nothing else running, nothing pinned
or isolated. Release build, latrec on. In a virtual machine waking a sleeping thread is slow (an
idle virtual CPU is halted and must be woken through the hypervisor), about 20 µs here against a
few on bare metal, so every hop that waits for a wakeup is inflated, and whatever avoids sleeping
gains more here than it would on bare metal. The absolute numbers are an upper bound; the order of
the links, and the size of each connector row, are what to read.

**Round trip, ASN.1, five rounds of 3000 round trips each** (median over the rounds of each round's
statistic, the range of the rounds in brackets, microseconds; CPU is the process, both ends, per
round trip; `busy` is `LIBE3_URING_BUSY_POLL_US=1000`, `sqpoll` is `LIBE3_URING_SQPOLL=1`, `plain` is
registered files off, `fixedbuf` registered buffers on):

| Transport | Link | Round trip p50 | p99 | Wire RAN to dApp p50 | Wire dApp to RAN p50 | Send indication p50 | Send control p50 | CPU |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| IPC | zmq | 208.2 [207.2-232.1] | 297.2 | 69.0 | 69.4 | 6.0 | 6.1 | 194 |
| IPC | posix | 110.5 [110.1-129.8] | 164.8 | 21.2 | 23.0 | 6.6 | 6.1 | 136 |
| IPC | uring | 111.9 [111.5-137.5] | 181.1 | 21.2 | 22.1 | 7.5 | 7.8 | 140 |
| IPC | uring, plain | 113.3 [110.5-154.5] | 196.9 | 21.9 | 22.1 | 7.9 | 8.1 | 140 |
| IPC | uring, fixedbuf | 113.4 [111.0-133.6] | 179.7 | 21.2 | 22.0 | 7.5 | 7.7 | 140 |
| IPC | uring, busy | 78.4 [78.2-79.1] | 172.6 | 6.1 | 5.9 | 7.1 | 7.0 | 325 |
| IPC | uring, sqpoll | 145.0 [144.0-158.2] | 264.5 | 17.8 | 16.8 | 22.7 | 21.2 | 505 |
| IPC | **uring, sqpoll + busy** | **19.4** [15.3-19.5] | 265.1 | 0.6 | 0.8 | 2.5 | 2.1 | 354 |
| TCP | zmq | 221.8 [220.9-226.7] | 325.8 | 74.2 | 75.4 | 6.1 | 6.3 | 209 |
| TCP | posix | 115.7 [113.5-118.3] | 174.6 | 20.1 | 20.2 | 10.5 | 10.2 | 141 |
| TCP | uring | 119.0 [118.5-119.3] | 179.7 | 19.5 | 20.8 | 11.5 | 11.9 | 145 |
| TCP | uring, plain | 118.7 [117.9-120.5] | 184.7 | 20.0 | 20.2 | 11.7 | 11.8 | 146 |
| TCP | uring, fixedbuf | 120.4 [119.5-122.0] | 185.3 | 19.8 | 21.3 | 11.7 | 12.0 | 148 |
| TCP | uring, busy | 84.1 [84.1-85.0] | 154.1 | 5.1 | 4.8 | 11.0 | 11.0 | 338 |
| TCP | uring, sqpoll | 165.7 [165.4-167.5] | 256.6 | 16.2 | 16.7 | 29.8 | 29.4 | 531 |
| TCP | **uring, sqpoll + busy** | **19.6** [18.0-21.2] | 305.5 | 0.2 | 0.3 | 3.8 | 3.4 | 378 |
| SCTP | posix | 122.0 [120.5-123.1] | 180.0 | 19.2 | 19.4 | 14.1 | 13.8 | 150 |
| SCTP | uring | 126.9 [126.0-128.1] | 192.7 | 20.1 | 18.8 | 15.6 | 15.3 | 156 |
| SCTP | uring, plain | 126.4 [126.2-127.2] | 186.3 | 20.0 | 18.4 | 15.6 | 15.5 | 156 |
| SCTP | uring, fixedbuf | 127.1 [125.8-130.3] | 198.7 | 19.9 | 18.2 | 15.7 | 15.4 | 157 |
| SCTP | uring, busy | 88.4 [88.2-88.8] | 149.8 | 3.5 | 3.3 | 13.9 | 14.3 | 355 |
| SCTP | uring, sqpoll | 176.9 [174.5-179.0] | 265.8 | 9.4 | 12.5 | 31.2 | 32.6 | 525 |
| SCTP | **uring, sqpoll + busy** | **34.3** [32.0-34.7] | 218.6 | 0.7 | 1.0 | 8.2 | 8.0 | 393 |

**The same with JSON and Protobuf**, three rounds of 2000, round trip p50 (the order of the links
does not change with the encoding):

| Transport | Encoding | posix | uring | uring, busy | uring, sqpoll + busy |
|---|---|---:|---:|---:|---:|
| IPC | JSON | 123.4 | 124.2 | 90.5 | **37.6** |
| IPC | Protobuf | 106.6 | 106.3 | 75.1 | **14.2** |
| TCP | JSON | 127.0 | 133.3 | 94.7 | **43.8** |
| TCP | Protobuf | 112.6 | 116.5 | 82.0 | **13.7** |

**Fastest: io_uring with the kernel submission thread and busy polling**, on every transport and
encoding, its slowest round faster than any other link's fastest. Next comes io_uring with busy
polling alone, 27 to 29% below POSIX. What each number is made of:

- **The connector's own rows.** Busy polling takes the receiver's wakeup out of the wire hop (about
  20 µs to 3.5 to 6 µs). The kernel submission thread makes the send a write to shared memory and the
  wire hop a read of it (a wire hop of 0.2 to 1.0 µs; a send of 2 to 8 µs against 6 to 14 for POSIX).
  Without busy polling the submission thread only adds a wakeup to every send (21 to 33 µs).
- **Not the connector's.** The two queuing hops in front of the outbound threads (rows 4 and 13 of the
  benchmark's table) cost about 19 µs each in every configuration but one: with the submission thread
  and busy polling, the next PDU reaches the outbound thread while it is still spinning in libe3's
  queue (40 spins, then 100 yields) and they fall to 1.4 µs, because no thread in the loop sleeps any
  more. That is most of the drop from 78 to 19 µs, and it holds only while PDUs come faster than the
  queue spins. Per phase, IPC, one run of 3000: POSIX 18.7 / 6.5 / 21.5 µs for queuing / send / wire
  of the indication, busy polling 19.9 / 7.2 / 6.2, submission thread and busy polling 1.3 / 2.4 / 0.6.
- **What does not matter.** Registered files and registered buffers made no difference beyond the
  spread of the rounds (the `plain` and `fixedbuf` rows); the send call of default io_uring costs about
  1 µs more than POSIX's `sendmsg` in the loop, 0.2 to 0.5 µs of it measured in isolation against the
  same plain receiver, and that is why io_uring without busy polling is 0 to 4% behind POSIX.
- **What it costs.** Busy polling 2.4 times POSIX's CPU, the submission thread and busy polling 2.6
  times, the submission thread alone 3.5 to 3.8 times. The p99 of the submission thread with busy
  polling is the worst of the fast variants: the spinning threads and the two kernel threads share six
  virtual CPUs with libe3's other threads.

**Run to run.** A first, preliminary run in a Docker Desktop virtual machine (10 virtual CPUs, more
threads around, one round of 500) agreed with the Arm virtual machine on everything but the order of
the two fastest variants: there the submission thread with busy polling took 86 µs and busy polling
alone 77 µs, as the spinning threads competed with the rest and the loop did not stay out of sleep.
The x86 server differs from both on three counts: the link with the submission thread and busy polling
was first there too, but with a lower p99 than POSIX's, not a higher one; the queuing rows play no part;
and default io_uring was not slower than POSIX beyond the noise. So the first place needs cores of its
own and a quiet machine; the second needs neither.

**Not measured:** bare metal with isolated cores (the x86 server was a virtual machine too); two hosts over a network; more than one dApp per RAN
(the batching of a broadcast); periodic rather than ping-pong traffic.

## Deferred, with their cost

| Item | Cost to the user | Status |
|---|---|---|
| Per-thread rings with `SINGLE_ISSUER` and `DEFER_TASKRUN` (decision 6) | A blocking call with a timeout on the RAN's inbound thread can return `EINTR` while receives are armed; each completion interrupts the waiting thread | Not scheduled |
| A dedicated I/O thread and an asynchronous `send()` | `send()` waits for the kernel as with POSIX | Not planned: it adds the thread hop the link removes |
| Multishot receive, provided-buffer rings, `SEND_ZC` (kernel 6.0 and later) | A receive is re-armed for each PDU; no zero-copy send for large PDUs (which would not help on loopback or IPC anyway) | Not scheduled; the probe is in place |
| NAPI busy polling (`IORING_REGISTER_NAPI`) | Busy polling stops at the socket; a real NIC still interrupts | Not scheduled; meaningless on loopback and IPC |
| Connector-level latrec stamps (submission, completion) | The send and wire rows cannot be split into submission, kernel and completion time | Unowned |
| The io_uring link in the Python wheels | `pip install libe3py` gets a library without the link (manylinux has no recent liburing); a Python dApp needs libe3 built from source | Unowned |
| A run on bare metal with isolated cores, and across two hosts | The numbers below come from a virtual machine and loopback | Unowned |
| `uring` in the dApp library and in OAI's E3 agent configuration | Their configuration parsers know `zmq` and `posix` only, so their users cannot pick the link yet; nothing breaks | A few lines in each; the dApp library's was run (its Python dApp over io_uring, against both links); twin pull requests not opened |
