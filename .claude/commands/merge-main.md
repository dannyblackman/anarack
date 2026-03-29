Merge the latest code from origin/main into the current branch.

## Steps

1. `git fetch origin main`
2. `git merge origin/main` — resolve conflicts if any

## Conflict Resolution

When conflicts occur:
1. Understand what our branch changed and why (check commit messages, conversation context)
2. Understand what main changed and why (check merged PR descriptions)
3. Resolve by preserving the intent of both sides

## After Successful Merge

Push the merge commit with `git push`.
