<!--
Thank you for contributing to libe3. Please fill in every section.
PRs that leave the mandatory boxes unchecked will not be reviewed.
-->

## Summary

<!-- 1–3 bullets describing what this PR changes and *why*. -->

-
-

## Type of change

- [ ] Bug fix
- [ ] New feature / enhancement
- [ ] New Service Model
- [ ] Refactor (no behavior change)
- [ ] Documentation
- [ ] Test / CI / packaging
- [ ] Other (explain):

## Linked issue

<!-- Required. Use "Closes #N" so the issue is auto-closed on merge. -->

Closes #

## Mandatory test checklist

These mirror what CI (`.github/workflows/pr-tests.yml`) enforces. **All boxes must be ticked before review.**

- [ ] `./build_libe3 -c -d build -j $(nproc) -r -t` passes (Release build + tests)
- [ ] `./build_libe3 -c -d build -j $(nproc) -g -t` passes (Debug build + tests)
- [ ] `cd build && ctest --output-on-failure` is clean
- [ ] MPMC queue benchmark (`./build/test_bench_mpmc_queue`) shows no regression vs `main`
- [ ] `VERSION` bumped per [SemVer](https://semver.org/) if the public API or ABI changed
- [ ] If public headers under `include/` were touched, `./build_libe3 --docs` renders without new Doxygen warnings
- [ ] If new build dependencies were added, they are installed by `./build_libe3 -I` (update the script if needed)
- [ ] If the `libe3.pc` interface changed, downstream consumers (`dApp-openairinterface5g`) still link cleanly

## CI checklist

- [ ] `Unit Tests` workflow is green (Debug + Release matrix on `ubuntu-latest`)
- [ ] `MPMC Queue Benchmark` job has posted results to this PR with no regression

## Twin-repo coordination

libe3 is paired with [`dapps`](https://github.com/wineslab/dApp-library) and [`dApp-openairinterface5g`](https://github.com/wineslab/dApp-openairinterface5g). **We do not accept patches that break or reduce compatibility with the twin repositories.**

- [ ] This PR does not change the E3 wire protocol or public ABI, OR a paired PR exists in each affected twin repo (link below).

Paired PR(s):

## Workflow confirmation

- [ ] This PR was opened against the **internal** repository (private). The public mirror `wineslab/dApp-libe3` is updated automatically by `.github/workflows/mirror.yml`.
- [ ] Commits will be **squashed** at merge time. Updates to this PR will be applied via **rebase** only — no merge commits, no duplicated history. (See `CONTRIBUTING.md` § Pull Request Process.)
- [ ] I have read and followed `CONTRIBUTING.md`.
