'use strict';
// Block skill usage when gstack is not installed globally.
//
// Vendored from gstack's team-init (required mode). Resolve the install root
// the way gstack skill preambles do: the GSTACK_ROOT env var first, then every
// host's global install location, then the migrated repo location. Block only
// when NONE exist, so Codex-host and migrated-repo installs are not blocked.
const fs = require('fs');
const os = require('os');
const path = require('path');

const home = process.env.HOME || process.env.USERPROFILE || os.homedir();
const roots = [process.env.GSTACK_ROOT || ''].concat([
  '.claude/skills/gstack', '.codex/skills/gstack', '.factory/skills/gstack', '.kiro/skills/gstack',
  '.config/opencode/skills/gstack', '.slate/skills/gstack', '.cursor/skills/gstack', '.openclaw/skills/gstack',
  '.hermes/skills/gstack', '.gbrain/skills/gstack', '.copilot/skills/gstack', '.gemini/antigravity-cli/skills/gstack', '.gemini/skills/gstack', '.gstack/repos/gstack',
].map((rel) => path.join(home, rel)));
const gstackRoot = roots.find((root) => {
  try { return root !== '' && fs.statSync(path.join(root, 'bin')).isDirectory(); } catch { return false; }
});

if (gstackRoot) {
  process.stdout.write('{}\n');
} else {
  process.stderr.write(`BLOCKED: gstack is not installed globally.

gstack is required for AI-assisted work in this repo.

Stop and ask the user to install it (do not install it yourself):
  git clone --depth 1 https://github.com/garrytan/gstack.git ~/.claude/skills/gstack
  cd ~/.claude/skills/gstack && ./setup --team

Then restart your AI coding tool.
`);
  // Exit 2 denies on Claude Code. The stdout JSON is the structured form of
  // the same deny.
  process.stdout.write(JSON.stringify({ hookSpecificOutput: { hookEventName: 'PreToolUse', permissionDecision: 'deny',
    permissionDecisionReason: 'gstack is required but not installed. See stderr for install instructions.' } }) + '\n');
  process.exitCode = 2;
}
