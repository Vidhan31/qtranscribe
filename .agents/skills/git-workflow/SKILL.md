---
name: git-workflow
description: Standard git branching, PR lifecycle, rescue procedures, safety rules, and branch cleanup workflows. Use when creating branches, pushing code, opening pull requests, rescuing accidental commits on main, syncing with main, or cleaning up merged branches.
---

# Git workflow

Standard procedures for Git branching, PR lifecycle, rescuing commits, and repository maintenance.

## Golden rules and pre-flight checks

Run these commands to inspect repository state before taking any action:

```bash
git status
git branch --show-current
git fetch origin
```

Core safety rules:
1. Never run destructive branch operations while on `main` (for example, `git reset --hard`, `git branch -D`).
2. Merge PRs on GitHub using Squash and merge by default.
3. Inspect diffs with `git diff main...HEAD` before opening a pull request.
4. Merge `origin/main` into your feature branch when syncing. Do not rebase published pull request branches by default.
5. Use short prefixes (`feat:`, `fix:`, `chore:`, `docs:`, `refactor:`, `test:`). Do not use long names like `feature:` or `bugfix:`.

## Quick reference

| Intent | Command |
| :--- | :--- |
| Check status and current branch | `git status && git branch --show-current` |
| Create and switch to new branch | `git switch -c feat/<short-description>` |
| Rescue uncommitted changes from `main` | `git switch -c feat/<short-description>` |
| Rescue committed changes from `main` | `git branch feat/<short-description> && git switch feat/<short-description> && git branch -f main origin/main` |
| Checkout existing pull request | `gh pr checkout <number>` |
| Inspect PR diff against main | `git diff main...HEAD` |
| Publish branch | `git push -u origin <branch>` |
| Open ready pull request | `gh pr create --title "feat: <title>" --body "<body>"` |
| Open draft pull request | `gh pr create --draft --title "feat: <title>" --body "<body>"` |
| Sync branch with main | `git fetch origin main && git merge origin/main && git push origin <branch>` |
| Clean up merged branch | `git switch main && git pull origin main && git fetch --prune && git branch -d <branch>` |

## Branch naming and creation

Create branches from an up-to-date `origin/main`.

### Naming conventions

Use short lowercase category prefixes with hyphens. Do not use full names like `feature/` or `bugfix/`.

- `feat/<short-description>`: New features or capabilities (not `feature/`)
- `fix/<short-description>`: Bug fixes (not `bugfix/`)
- `refactor/<short-description>`: Code changes that neither fix a bug nor add a feature
- `chore/<short-description>`: Build scripts, tool configs, or dependencies
- `docs/<short-description>`: Documentation changes (not `documentation/`)
- `test/<short-description>`: Test additions or test modifications

### Creating a branch

```bash
git switch main
git pull origin main
git switch -c feat/<short-description>
```

## Rescuing work from main

If changes were made directly on `main`, first check the working tree and commit log:

```bash
git status
git log origin/main..HEAD --oneline
```

Choose the rescue method based on the output.

### Case 1: Uncommitted changes

If files are modified or staged, but not committed to `main`:

```bash
git switch -c feat/<short-description>
```

This creates the branch and moves uncommitted changes with you.

### Case 2: Accidental commits on main

If you made local commits directly on `main` that are ahead of `origin/main`:

```bash
# 1. Create and switch to the new feature branch at current HEAD
git branch feat/<short-description>
git switch feat/<short-description>

# 2. Reset local main back to origin/main without switching back
git branch -f main origin/main
```

Verify `main` is restored:

```bash
git log main..HEAD --oneline
```

## Working with existing pull requests

### Using GitHub CLI (preferred)

```bash
gh pr checkout <number>
```

### Manual fetch fallback

If `gh` is unavailable:

```bash
git fetch origin pull/<number>/head:pr/<number>
git switch pr/<number>
```

## Pre-PR checks and review

Run quality checks and review the full diff before opening a pull request.

### 1. Code checks

```bash
pre-commit run --all-files
cmake --build build
```

### 2. Diff inspection

Compare the tip of the feature branch to the merge base on `main`:

```bash
git diff main...HEAD
```

Verify that only intended files and changes are present.

## Creating and updating pull requests

### 1. Commit naming format

Always format commit messages and PR titles with short prefix conventions:

- `feat: <concise summary>`
- `fix: <concise summary>`
- `refactor: <concise summary>`
- `chore: <concise summary>`
- `docs: <concise summary>`
- `test: <concise summary>`

### 2. Push branch to remote

```bash
git push -u origin feat/<short-description>
```

### 3. Create pull request

Use a short prefix title and structured body:

```bash
gh pr create --title "feat: <concise description>" --body "## Summary
<Brief description of change>

## Changes Made
- <Item 1>
- <Item 2>

## Verification
- <Steps taken to test>"
```

Add `--draft` if the PR is still a work in progress:

```bash
gh pr create --draft --title "feat: <concise description>" --body "<body>"
```

If `gh` is not available, push the branch and open the URL printed by Git in the terminal.

### 4. Update existing pull request

To add changes to an open PR, commit locally with a short prefix and push:

```bash
git add <files>
git commit -m "fix: <message>"
git push origin <branch>
```

## Branch synchronization

Keep the feature branch updated by merging `origin/main`. Do not rebase branches that have open pull requests.

```bash
git fetch origin main
git merge origin/main
```

Resolve any conflicts, then push:

```bash
git push origin <branch>
```

## Post-merge cleanup

After a pull request merges on GitHub:

### 1. Update main and prune remote tracking references

```bash
git switch main
git pull origin main
git fetch --prune
```

### 2. Delete local branch

```bash
git branch -d feat/<short-description>
```

If Git warns the branch is not fully merged (common when using squash merges on GitHub), verify the PR merged on GitHub, then delete with `git branch -D feat/<short-description>`.

### 3. Delete remote branch (if still present)

If GitHub auto-delete did not remove the remote branch:

```bash
git push origin --delete feat/<short-description>
```

### 4. Clean stale tracking references

Remove local tracking references to remote branches that were deleted:

```bash
git branch -vv | grep ': gone]' | awk '{print $1}' | xargs -r git branch -d
```
