# Container images

Four images that build libe3 once and run it as a simulated RAN, a simulated
dApp, or the latency benchmark. Each image is a plain `docker build`, so the
same files serve a laptop, a CI job and a cluster pipeline.

| Image | File | Runs |
|---|---|---|
| `libe3-build` | `Dockerfile.build` | Nothing. It holds the source, the toolchain and the build with every encoding, SCTP, the io_uring link, the integration tests and latrec on. The others copy from it. |
| `libe3-ran-sim` | `Dockerfile.ran-sim` | `example_simple_agent`, a RAN E3 termination with the reference Service Model |
| `libe3-dapp-sim` | `Dockerfile.dapp-sim` | `example_simple_dapp`, a dApp E3 termination |
| `libe3-bench` | `Dockerfile.bench` | `test_bench_full_loop_latency`, the full-loop latency benchmark |

The dependency layer of `libe3-build` depends only on `build_libe3` and
`VERSION`, so a source change rebuilds the library but not the toolchain or
asn1c. `build_libe3 -I` stays the one place that lists the dependencies.

## Build

From the root of the repository:

```
docker build -f docker/Dockerfile.build    -t libe3-build:local .
docker build -f docker/Dockerfile.ran-sim  -t libe3-ran-sim:local  .
docker build -f docker/Dockerfile.dapp-sim -t libe3-dapp-sim:local .
docker build -f docker/Dockerfile.bench    -t libe3-bench:local    .
```

The last three take `--build-arg BUILD_IMAGE=<image>` to copy from another
build of `libe3-build`.

## Run a RAN and a dApp together

```
docker/run-pair.sh posix sctp asn1
```

`run-pair.sh LINK TRANSPORT ENCODING` starts the RAN, then the dApp in the same
network namespace, with one shared directory for IPC. It passes when the dApp
exits 0 and prints at least one indication. The endpoints and ports are the
library defaults; the arguments after the image name are those of the examples
(`--link zmq|posix|uring`, `--transport ipc|tcp|sctp`, `--encoding asn1|json|protobuf`).

SCTP needs the `sctp` kernel module on the machine that runs the containers:
containers share the host kernel.

The io_uring link (`--link uring`) needs the kernel to let the container use
io_uring. Docker's default seccomp profile refuses it, so `run-pair.sh` starts
both containers with `--security-opt seccomp=unconfined` for that link; anywhere
else, run them that way or with a profile that allows `io_uring_setup`,
`io_uring_enter` and `io_uring_register`. The benchmark image needs the same:

```
docker run --rm --security-opt seccomp=unconfined libe3-bench:local --link uring
```

## Run the benchmark

```
docker run --rm libe3-bench:local
```
