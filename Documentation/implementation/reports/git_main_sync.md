# Main synchronization and pull policy

2026-10-19. The `Blood2/` checkout had six local commits through `2322e02`
and nine distinct remote commits through `origin/main` at `1f9f9db`, from their
common `d149d32` ancestor. Its repository-local `pull.ff=only` rejected a normal
pull because neither tip could advance directly to the other.

The user requested repair of this checkout. A merge preserves the existing
launcher commit identities and incorporates the remote campaign, inventory,
test and Apple build changes. The pre-merge local tip is retained at
`codex/backup-before-main-sync-20261019`. The only conflict was appended sections
of `docs/implementation/provenance.md`; both were retained. The remote README's
single trailing space was removed. Original/reference repositories and the
pre-existing workspace-root README edits are untouched.

The repository-local configuration now contains:

```sh
git config --local pull.ff true
git config --local pull.rebase false
```

Future pulls can fast-forward when possible and merge divergent histories while
preserving commits. This does not eliminate genuine content conflicts, and it
does not change global Git settings or other checkouts. Repository configuration
is local and is not carried to another machine by a clone/pull.

The merge is Blood2 commit `b13b2c7`, with parents `2322e02` and `1f9f9db`.
Both the previous local tip and `origin/main` are verified ancestors. A subsequent
[`git pull --no-edit`](evidence/git-main-sync-pull.log) returns `Already up to date.`
The Blood2 working tree is clean, with seven commits ahead and zero behind the
fetched `origin/main`; this operation did not push those commits.

Validation on Linux:

- Release [configure](evidence/git-main-sync-configure-release.log) and
  [build of `blood2_app` and `config_tests`](evidence/git-main-sync-build-release.log)
  pass. The game target also rebuilds/deploys its isolated launcher.
- Configuration and launcher protocol checks pass
  [8/8](evidence/git-main-sync-headless.log).
- Actual retail and demo menu traversal in the rebuilt game passes
  [2/2](evidence/git-main-sync-gpu.log), using the local NVIDIA Vulkan ICD and
  isolated XDG state.
- Merge markers, unmerged entries and whitespace checks are clear. The launcher
  isolation/default-build changes coexist with the remote Apple notification
  dependency fix. Upstream rendering/gameplay source was retained, with no manual
  code conflict resolution.

These are integration checks for the merge, not a fresh qualification of the
full remote campaign work or a new platform/M2/M3 acceptance.
