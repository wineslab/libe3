# Container images

Five images that build libe3 once and run it as a simulated RAN, a simulated
dApp, or the latency benchmark. Each image is a plain `docker build`, so the
same files serve a laptop, a CI job and a cluster pipeline.

| Image | File | Runs |
|---|---|---|
| `libe3-deps` | `Dockerfile.deps` | Nothing. The packages and asn1c that libe3 builds with, and none of libe3. |
| `libe3-build` | `Dockerfile.build` | Nothing. It starts from `libe3-deps` and holds the source and the build with every encoding, SCTP, the integration tests and latrec on. The others copy from it. |
| `libe3-ran-sim` | `Dockerfile.ran-sim` | `example_simple_agent`, a RAN E3 termination with the reference Service Model |
| `libe3-dapp-sim` | `Dockerfile.dapp-sim` | `example_simple_dapp`, a dApp E3 termination |
| `libe3-bench` | `Dockerfile.bench` | `test_bench_full_loop_latency`, the full-loop latency benchmark |

`libe3-deps` depends only on `build_libe3` and `Dockerfile.deps`, so it is built
when those change and a source change rebuilds only `libe3-build`. A CI pipeline
that keeps `libe3-deps` in a registry under a hash of those two files does not
download packages on every run. `build_libe3 -I` stays the one place that lists
the dependencies.

## Build

From the root of the repository:

```
docker build -f docker/Dockerfile.deps     -t libe3-deps:local  .
docker build -f docker/Dockerfile.build    -t libe3-build:local .
docker build -f docker/Dockerfile.ran-sim  -t libe3-ran-sim:local  .
docker build -f docker/Dockerfile.dapp-sim -t libe3-dapp-sim:local .
docker build -f docker/Dockerfile.bench    -t libe3-bench:local    .
```

`Dockerfile.build` takes `--build-arg DEPS_IMAGE=<image>` to start from another
build of `libe3-deps`, and `Dockerfile.deps` takes `--build-arg UBUNTU_MIRROR=<url>`
to replace `http://archive.ubuntu.com/ubuntu/`. The last three take
`--build-arg BUILD_IMAGE=<image>` to copy from another build of `libe3-build`.

## Run a RAN and a dApp together

```
docker/run-pair.sh posix sctp asn1
```

`run-pair.sh LINK TRANSPORT ENCODING` starts the RAN, then the dApp in the same
network namespace, with one shared directory for IPC. It passes when the dApp
exits 0 and prints at least one indication. The endpoints and ports are the
library defaults; the arguments after the image name are those of the examples
(`--link zmq|posix`, `--transport ipc|tcp|sctp`, `--encoding asn1|json|protobuf`).

SCTP needs the `sctp` kernel module on the machine that runs the containers:
containers share the host kernel.

## Run the benchmark

```
docker run --rm libe3-bench:local
```
