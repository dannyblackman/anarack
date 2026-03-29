---
name: release
description: Safe release workflow — merge feature branch to main with checks
---

# Release Workflow

This skill guides you through merging a feature branch to main safely.

## Pre-Release Checklist

Before starting, verify:

1. **You're on a feature branch** (not main)
2. **All changes are committed and pushed**
3. **CI has passed on the PR** (if GitHub Actions are set up)
4. **PR exists** (create one if not)

## Step 1: Gather Current State

```bash
CURRENT_BRANCH=$(git branch --show-current)
echo "Current branch: $CURRENT_BRANCH"

if [ "$CURRENT_BRANCH" = "main" ]; then
  echo "ERROR: Already on main. Switch to a feature branch first."
  exit 1
fi

if ! git diff --quiet || ! git diff --cached --quiet; then
  echo "WARNING: Uncommitted changes."
  git status --short
fi

PR_NUMBER=$(gh pr view --json number -q '.number' 2>/dev/null || echo "")
if [ -z "$PR_NUMBER" ]; then
  echo "No PR found — create one first: gh pr create"
else
  echo "PR #$PR_NUMBER"
fi
```

## Step 2: Check PR Status

```bash
gh pr checks
```

If checks are failing, stop and fix first.

## Step 3: Merge

```bash
gh pr merge --merge
```

Use `--merge` (not squash) to preserve commit history.

## Step 4: Clean Up Worktree

If releasing from a worktree:

1. **cd to the main repo root FIRST** (critical — don't delete a worktree you're inside)

```bash
cd /Users/Danny_1/Sites/Anarack
git worktree remove --force worktrees/<branch-name>
```

2. Delete the remote branch if not auto-deleted:

```bash
git push origin --delete <branch-name>
```

## Step 5: Monitor

```bash
gh run watch
```
