# Codex Job History

This file records user-requested work completed in this workspace. Job `000` is reserved for this history.

## Index

| ID | Date | Requested job | Outcome |
|---|---|---|---|
| 001 | 2026-08-16 | Document the repository in detail | Completed |
| 002 | 2026-08-16 | Prefix root Markdown files by creation sequence | Completed |

## Job 001 — Detailed repository documentation

- **Date:** 2026-08-16
- **Request:** Document the repository in detail.
- **Work:**
  - Mapped the CMake targets, Qt/OpenGL application architecture, coordinate spaces, rendering and interaction paths.
  - Documented JSON persistence, Qt/browser synchronization, browser API/file management, semantic playback, assets, and existing design notes.
  - Reconfigured and rebuilt the C++ project, ran both CTest targets, ran all five playback tests, compiled both Python tools, and dry-ran the eight-action example.
  - Recorded current coverage gaps, repository hygiene concerns, stale documentation risks, and recommended development workflows.
- **Outcome:** Completed. The consolidated developer guide reflects the checked-out `Auto-Demo-Script-Development` branch and distinguishes implemented behavior from historical plans.
- **Artifacts:** [Job-001_cdx-repository-guide.md](Job-001_cdx-repository-guide.md)

## Job 002 — Sequence-prefix root Markdown files

- **Date:** 2026-08-16
- **Request:** Rename root `*.md` files with a three-digit prefix that hints at their creation sequence.
- **Work:**
  - Derived creation order from each tracked Markdown file's first-add Git commit timestamp.
  - Used alphabetical ordering as a deterministic tie-break for files added in the same commit.
  - Renamed 25 root Markdown files with prefixes `001-` through `025-` and updated Markdown cross-references.
  - Preserved `CJ_cdx/Job-*` filenames because they follow the job-history naming contract.
- **Outcome:** Completed. Repository-root Markdown names now expose their approximate creation sequence.

---

## Append template

```markdown
| NNN | YYYY-MM-DD | Concise requested job | Completed |

## Job NNN — Short title

- **Date:** YYYY-MM-DD
- **Request:** Concise wording of the user's request.
- **Work:** Important actions, decisions, and verification.
- **Outcome:** Completed, partially completed, blocked, or explained.
- **Artifacts:** Repository-relative links to files created or changed, when applicable.
```
