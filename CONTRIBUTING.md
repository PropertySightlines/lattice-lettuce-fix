# Contributing to Salt and Lattice

Welcome! This repository tracks the development of the Salt programming language, its compiler `salt-front`, and the Lattice Operating System ecosystem (Kernel, NetD, baselines).

## Versioning Policy: The "Sovereign Distribution" Model

Lattice is built as a cohesive, sovereign platform. As such, we use a **Unified Versioning Strategy** for all repository-wide Git releases (e.g., `v0.9.0`). 

When a user pulls a Lattice release, that tag guarantees that a specific version of the Salt compiler is verified to build a specific version of the Lattice Kernel, the Ring 3 NetD daemon, and the Socket API ecosystem.

### 1. The Unified Git Tag
Every time there is a major architectural milestone (such as moving the networking stack to Ring 3 or completing a major pass of Z3 proofs), we cut a unified repository tag. 
* Example: `v0.9.0` represents the "Unified Ring 3 Networking" milestone.

### 2. Internal Component Versions
While the Git tag tracks the state of the unified platform, individual sub-systems (like the Kernel, standard library, and downstream applications like Basalt) track their own internal maturity versions. 
* These are actively tracked in the `manifest.salt` file at the root of the repository.
* When the platform version is bumped, `manifest.salt` is used by the build tools to verify that the internal components are synchronized.

### 3. Kernel Version Identification
The kernel binary maintains its own version constant to report during the boot screen (e.g., `LATTICE BOOT [OK] v0.9.0`), decoupled from the compiler version used to build it.

## Submitting Pull Requests
1. All changes to the kernel or standard library *must* pass the Test-Driven Development (TDD) gates. Do not submit a PR unless `tools/test_local.py` or `tools/runner_qemu.py` reports GREEN for your gates.
2. If your change affects cross-component compatibility (e.g., changing the IPC contract between the Kernel and NetD), you must update both components in the same atomic PR.
3. If you introduce a new system service or major application, propose adding it to `manifest.salt` in your PR description.
