---
name: cleanup-branches
description: Clean up git worktrees and stale branches for merged/deleted branches
---

# Branch and Worktree Cleanup

## Step 1: Gather Data

Run in parallel:

1. **List git branches merged into main**:
   ```bash
   git branch -r --merged main | grep -v 'origin/main' | grep -v 'origin/HEAD'
   ```

2. **List all remote branches**:
   ```bash
   git branch -r | grep -v 'origin/HEAD' | sed 's/origin\///' | sort
   ```

3. **List git worktrees**:
   ```bash
   git worktree list
   ```

## Step 2: Present

For each worktree (excluding main), check if the branch still exists remotely and categorise as:
- **Safe to delete**: Remote branch gone or merged
- **Keep**: Branch still active

## Step 3: Get User Confirmation

Ask before deleting anything.

## Step 4: Execute

**CRITICAL**: Always `cd /Users/Danny_1/Sites/Anarack` BEFORE removing worktrees.

```bash
cd /Users/Danny_1/Sites/Anarack
git worktree remove --force <worktree_path>
git push origin --delete <branch-name>  # if remote branch still exists
```

## Step 5: Verify

```bash
git worktree list
git branch -r
```
