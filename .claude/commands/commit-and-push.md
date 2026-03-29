Before committing, check for ALL modified and new files with `git status --short`.

Handle .claude configuration files intelligently:
- ONLY commit .claude/ files if they were explicitly part of the user's request or conversation
- Do NOT commit .claude/ changes that are just temporary setup
- If .claude/ files are staged but weren't discussed, use `git reset .claude/` to unstage them

Staging strategy:
- Use `git add` to stage relevant changes to this conversation
- Include modified files even if not directly discussed IF they're dependencies or related changes

Generate a concise yet descriptive commit message summarising the changes made.

## Push to Remote

Push to the remote repository after committing.

Safety checks:
- Show git diff --stat before committing to verify what's included

Summarise:
- what got done
- anything that has not been committed
