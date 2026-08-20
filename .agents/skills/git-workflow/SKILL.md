---
name: git-workflow
description: Use when asked to write or edit code, add features, fix bugs, update docs, or perform Git tasks (branching off origin/main, committing on request, syncing, pushing, rescuing work from main, or cleaning up merged branches).
---

# Git workflow

Operational Git procedures for agents when modifying files, managing branches, creating commits, and syncing with remotes.

## Quick reference

| Scenario | Agent action | Commands |
| :--- | :--- | :--- |
| Starting task from `main` | Pull latest and branch off | `git switch main && git pull origin main && git switch -c <type>/<name>` |
| Unrelated task on feature branch | Switch to `main`, pull, and branch off | `git switch main && git pull origin main && git switch -c <type>/<name>` |
| Related follow-up task | Stay on current branch | Continue on active branch |
| Uncommitted edits on `main` | Move changes to new branch | `git switch -c <type>/<name>` |
| Accidental commits on `main` | Branch at HEAD, restore `main` | `git branch <type>/<name> && git switch <type>/<name> && git branch -f main origin/main` |
| User explicitly asks to commit | Stage and conventional commit | `git add <files> && git commit -m "<type>: <summary>"` |
| Sync branch with main | Merge `origin/main` | `git fetch origin main && git merge origin/main` |
| Push branch | Push with upstream tracking | `git push -u origin <branch>` |
| Clean up merged branch | Delete local and prune refs | `git switch main && git pull origin main && git fetch --prune && git branch -d <branch>` |

## Safety rules

1. Never run destructive operations on `main` (`git reset --hard`, `git branch -D`).
2. Never commit directly to `main`.
3. Never force push (`git push -f`) to `main` or shared remote branches.
4. Stage and commit only when the user explicitly asks to commit in the prompt.

## Phase 1. Pre-flight checks

Inspect repository status before editing any files in the workspace:

```bash
git status
git branch --show-current
git fetch origin
```

Check:
- [ ] Active branch identified.
- [ ] Working tree status clean or noted.
- [ ] Remote tracking status known.

## Phase 2. Branching protocol

Determine the target branch before modifying project files.

### 1. If currently on main

Create and switch to a topic branch from latest `origin/main` before touching files:

```bash
git switch main
git pull origin main
git switch -c <type>/<short-description>
```

### 2. If currently on a topic branch

- **Related task:** If the prompt is a continuation, iterative refinement, or bug fix for the current branch, stay on the branch.
- **Unrelated task:** Switch to `main`, update from remote, and create a fresh branch:

```bash
git switch main
git pull origin main
git switch -c <type>/<short-description>
```

### 3. Branch naming conventions

Use lowercase category prefixes with hyphenated kebab-case descriptions:

- `feat/<topic>`: New features or capabilities (do not use `feature/`)
- `fix/<topic>`: Bug fixes (do not use `bugfix/`)
- `refactor/<topic>`: Structural code improvements without behavior change
- `chore/<topic>`: Tooling, dependency updates, and build configs
- `docs/<topic>`: Documentation changes (do not use `documentation/`)
- `test/<topic>`: Test additions or test modifications

## Phase 3. Rescuing work from main

If changes were made directly on `main`, inspect the working tree and log:

```bash
git status
git log origin/main..HEAD --oneline
```

### Case 1. Uncommitted changes on main

Move unstaged or staged modifications directly to a new topic branch:

```bash
git switch -c <type>/<short-description>
```

### Case 2. Accidental commits on main

When local commits exist on `main` ahead of `origin/main`:

```bash
# 1. Create and switch to the branch at current HEAD
git branch <type>/<short-description>
git switch <type>/<short-description>

# 2. Reset local main back to origin/main without switching back
git branch -f main origin/main
```

Verify local `main` matches upstream:

```bash
git log main..HEAD --oneline
```

## Phase 4. Staging and committing changes

Do not create commits automatically after modifying files. Only stage and commit when the user explicitly requests a commit in their prompt.

When requested, use conventional commit format matching the branch prefix:

```bash
git add <files>
git commit -m "<type>: <imperative summary>"
```

### Commit types and format

- `feat: <summary>` for new functionality
- `fix: <summary>` for bug fixes
- `refactor: <summary>` for structural refactoring
- `chore: <summary>` for build scripts, configs, and dependencies
- `docs: <summary>` for documentation edits
- `test: <summary>` for tests

Use lowercase, imperative mood in summaries (e.g., `feat: add audio stream buffer`, not `feat: Added audio stream buffer`).

## Phase 5. Branch synchronization and conflict resolution

Keep long-running feature branches synchronized with upstream `main`:

```bash
git fetch origin main
git merge origin/main
```

### Conflict resolution rules

- **Non-overlapping conflicts:** For clear, non-overlapping changes across files or distinct code blocks, resolve the conflict markers, run relevant build or test checks, and stage the resolved files.
- **Ambiguous or architectural conflicts:** If the conflict involves competing design choices or unclear intent, pause execution, show the conflicting diffs, and ask the user for direction before completing the merge.

## Phase 6. Pushing branches

When the user asks to push changes to the remote repository:

```bash
# Push new branch with upstream tracking
git push -u origin <branch-name>

# Update existing pushed branch
git push origin <branch-name>
```

## Phase 7. Post-merge branch cleanup

After branch work is merged into `main` and active work is complete:

### 1. Update main and prune remotes

```bash
git switch main
git pull origin main
git fetch --prune
```

### 2. Delete local branch

```bash
git branch -d <type>/<short-description>
```

If Git indicates the branch is not fully merged (common with upstream squash merges), confirm `main` contains the changes, then force-delete:

```bash
git branch -D <type>/<short-description>
```

### 3. Delete remote branch (when requested)

```bash
git push origin --delete <type>/<short-description>
```

### 4. Clean stale tracking references

```bash
git branch -vv | grep ': gone]' | awk '{print $1}' | xargs -r git branch -d
```
