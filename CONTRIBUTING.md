# Contributing

Thank you for considering contributing to this project! Please follow these guidelines to help us maintain a welcoming and productive environment.
When contributing to this repository, please first discuss the change you wish to make via issue,
email, or any other method with the original authors of this work. 

Please note we have a code of conduct, please follow it in all your interactions with the project.

## Pull Request Process

1. Ensure any install or build dependencies are removed before the end of the layer when doing a 
   build.
2. Update the README.md with details of changes to the interface, this includes new environment 
   variables, exposed ports, useful file locations and container parameters.
3. Increase the version numbers in any examples files and the README.md to the new version that this
   Pull Request would represent. The versioning scheme we use is [SemVer](http://semver.org/). 
   This repository is part of a framework that includes a Python library for dApps and a fork of OpenAirInterface written in C.
   We do not accept major patches that break or reduce compatibility with the twin repository.
4. Submitted code must have been tested and should work with all the compatible configurations of the framework.
5. Merging should always be performed via squash commits. Pull Requests must be updated through rebasing, without duplicating commits or altering the original branch history. Merges without prior approval are not allowed, and any accidental violations will result in the deletion of the merge commit.

## Development Workflow

Active development happens on a **private internal repository**. The **public** repository **`wineslab/dApp-libe3`** is a 1:1 mirror of that internal repo, updated automatically by `.github/workflows/mirror.yml` on every push to `main`. **Do not open pull requests against the public mirror — they will not be merged.**

### For maintainers and existing collaborators

Fork or branch from the internal repository, open the PR there, follow the templates in `.github/`, and let CI run.

### For external contributors

External contributors must request access to the internal repository before opening a PR. Two channels are accepted:

1. **Preferred:** open an issue on the public mirror at https://github.com/wineslab/dApp-libe3/issues using the *"Request access to the internal development repository"* contact link (or any template with the `access-request` label). Tell us briefly what you want to work on.
2. **Fallback:** email **a.lacava@northeastern.edu** with the same information.

Once access is granted, you will be invited to the internal repository; fork it from there, push your branch, and open the PR against the internal repo.

## Mandatory checks

The following are **mandatory** for every contribution. PRs that do not meet them will not be reviewed.

### Issues

- All issues must use one of the templates in `.github/ISSUE_TEMPLATE/` (`bug_report`, `feature_request`, `question`, `documentation`, or `new_sm`). Blank issues are disabled.
- Required fields in the templates must be completed; placeholder text is not acceptable.

### Pull requests

- All PRs must use `.github/PULL_REQUEST_TEMPLATE.md` and complete every checklist item.
- The following CI workflow must be green on the latest commit before review:
  - **`Unit Tests`** (`.github/workflows/pr-tests.yml`) — builds and runs `ctest --output-on-failure` for both `Debug` and `Release` on `ubuntu-latest`, then runs the MPMC queue benchmark and posts results as a PR comment.
- The following local checks must be reported in the PR description as run by the contributor (mirroring CI):
  - `./build_libe3 -c -d build -j $(nproc) -r -t` passes (Release + tests).
  - `./build_libe3 -c -d build -j $(nproc) -g -t` passes (Debug + tests).
  - The MPMC benchmark (`./build/test_bench_mpmc_queue`) shows no regression vs `main`.
  - `VERSION` bumped per [SemVer](https://semver.org/) when public API or ABI changes.
  - `./build_libe3 --docs` renders without new Doxygen warnings if public headers under `include/` were modified.
  - `README.md` and/or `CONTRIBUTING.md` updated when contributor- or user-facing behavior changes.

### Service Models and twin-repo coordination

This repository is part of a framework that includes [`dapps`](https://github.com/wineslab/dApp-library) (the Python dApp library) and [`dApp-openairinterface5g`](https://github.com/wineslab/dApp-openairinterface5g) (the OAI fork). **We do not accept patches that break or reduce compatibility with the twin repositories.** PRs that change the public API/ABI, the E3 wire protocol, or add a new Service Model must come with a paired PR in the affected twin repo, linked from the PR description.

## Code of Conduct

### Our Pledge

In the interest of fostering an open and welcoming environment, we as
contributors and maintainers pledge to making participation in our project and
our community a harassment-free experience for everyone, regardless of age, body
size, disability, ethnicity, gender identity and expression, level of experience,
nationality, personal appearance, race, religion, or sexual identity and
orientation.

### Our Standards

Examples of behavior that contributes to creating a positive environment
include:

* Using welcoming and inclusive language
* Being respectful of differing viewpoints and experiences
* Gracefully accepting constructive criticism
* Focusing on what is best for the community
* Showing empathy towards other community members

Examples of unacceptable behavior by participants include:

* The use of sexualized language or imagery and unwelcome sexual attention or
advances
* Trolling, insulting/derogatory comments, and personal or political attacks
* Public or private harassment
* Publishing others' private information, such as a physical or electronic
  address, without explicit permission
* Other conduct which could reasonably be considered inappropriate in a
  professional setting

### Our Responsibilities

Project maintainers are responsible for clarifying the standards of acceptable
behavior and are expected to take appropriate and fair corrective action in
response to any instances of unacceptable behavior.

Project maintainers have the right and responsibility to remove, edit, or
reject comments, commits, code, wiki edits, issues, and other contributions
that are not aligned to this Code of Conduct, or to ban temporarily or
permanently any contributor for other behaviors that they deem inappropriate,
threatening, offensive, or harmful.

### Scope

This Code of Conduct applies both within project spaces and in public spaces
when an individual is representing the project or its community. Examples of
representing a project or community include using an official project e-mail
address, posting via an official social media account, or acting as an appointed
representative at an online or offline event. Representation of a project may be
further defined and clarified by project maintainers.

### Enforcement

Instances of abusive, harassing, or otherwise unacceptable behavior may be
reported by contacting the project team. All
complaints will be reviewed and investigated and will result in a response that
is deemed necessary and appropriate to the circumstances. The project team is
obligated to maintain confidentiality with regard to the reporter of an incident.
Further details of specific enforcement policies may be posted separately.

Project maintainers who do not follow or enforce the Code of Conduct in good
faith may face temporary or permanent repercussions as determined by other
members of the project's leadership.

### Attribution

This Code of Conduct is adapted from the [Contributor Covenant][homepage], version 1.4,
available at [http://contributor-covenant.org/version/1/4][version]

[homepage]: http://contributor-covenant.org
[version]: http://contributor-covenant.org/version/1/4/