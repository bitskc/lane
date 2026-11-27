'use strict';
// Smoke test for the gstack gate. Runs the exact command registered in
// .claude/settings.json (through sh, like Claude Code) against throwaway HOME
// and GSTACK_ROOT directories. Not part of CI: `node .claude/hooks/check-gstack.smoke.cjs`.
// POSIX only (uses sh).
const assert = require('assert');
const { spawnSync } = require('child_process');
const fs = require('fs');
const os = require('os');
const path = require('path');

const repo = path.resolve(__dirname, '..', '..');
const settings = JSON.parse(fs.readFileSync(path.join(repo, '.claude', 'settings.json'), 'utf8'));
const [entry] = settings.hooks.PreToolUse;
const [handler] = entry.hooks;
assert.strictEqual(entry.matcher, 'Skill|skill');
assert.strictEqual(handler.onFailure, 'block');

const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'check-gstack-'));
const mk = (...p) => { const d = path.join(tmp, ...p); fs.mkdirSync(d, { recursive: true }); return d; };

function run(env, cwd) {
  // Real node first on PATH: version-manager shims can resolve node through HOME.
  const nodePath = path.dirname(process.execPath) + path.delimiter + process.env.PATH;
  const base = { PATH: nodePath, HOME: path.join(tmp, 'home'), CLAUDE_PROJECT_DIR: repo };
  return spawnSync('/bin/sh', ['-c', handler.command], { env: { ...base, ...env }, cwd, encoding: 'utf8' });
}

try {
  mk('home');
  const binIsFile = mk('binfile');
  fs.writeFileSync(path.join(binIsFile, 'bin'), '');

  let r = run({ GSTACK_ROOT: path.dirname(mk('installed', 'bin')) });
  assert.strictEqual(r.status, 0, 'GSTACK_ROOT install allows');
  assert.strictEqual(r.stdout.trim(), '{}');

  // Installs found through HOME, with no GSTACK_ROOT.
  let installedHome;
  for (const rel of ['.claude/skills/gstack', '.codex/skills/gstack', '.gstack/repos/gstack']) {
    const home = mk('home-' + rel.replace(/\W/g, '_'));
    mk('home-' + rel.replace(/\W/g, '_'), rel, 'bin');
    r = run({ HOME: home });
    assert.strictEqual(r.status, 0, `${rel} under HOME allows`);
    installedHome ||= home;
  }

  // A stale GSTACK_ROOT must fall through to the HOME locations, not block.
  r = run({ HOME: installedHome, GSTACK_ROOT: binIsFile });
  assert.strictEqual(r.status, 0, 'stale GSTACK_ROOT falls through to HOME install');
  assert.strictEqual(r.stdout.trim(), '{}');

  // path.join('', 'bin') is the relative path 'bin'. Without the empty-root
  // guard, an unset or empty GSTACK_ROOT would match a cwd that holds a bin/,
  // end the search with '' (falsy) and block despite a real install under HOME.
  const cwdWithBin = path.dirname(mk('cwd-with-bin', 'bin'));
  for (const [name, env] of [
    ['unset GSTACK_ROOT', { HOME: installedHome }],
    ['empty GSTACK_ROOT', { HOME: installedHome, GSTACK_ROOT: '' }],
  ]) {
    r = run(env, cwdWithBin);
    assert.strictEqual(r.status, 0, `${name} with a bin/ in cwd still finds the HOME install`);
    assert.strictEqual(r.stdout.trim(), '{}');
  }

  // Each deny case runs from a plain cwd and from one that holds a bin/: a
  // relative 'bin' must never count as an install.
  for (const [name, env] of [
    ['no install anywhere', {}],
    ['empty GSTACK_ROOT', { GSTACK_ROOT: '' }],
    ['bin is a file', { GSTACK_ROOT: binIsFile }],
  ]) {
    for (const cwd of [undefined, cwdWithBin]) {
      const label = cwd ? `${name}, cwd holds bin/` : name;
      r = run(env, cwd);
      assert.strictEqual(r.status, 2, `${label} denies`);
      assert.match(r.stderr, /BLOCKED: gstack is not installed globally/);
      // Claude Code shows the JSON reason to the model instead of stderr, so the
      // reason must be the same full message: install steps and the
      // do-not-install-yourself rule included.
      const { permissionDecision, permissionDecisionReason } = JSON.parse(r.stdout).hookSpecificOutput;
      assert.strictEqual(permissionDecision, 'deny');
      assert.strictEqual(permissionDecisionReason, r.stderr, `${label}: reason equals stderr`);
      assert.match(permissionDecisionReason, /do not install it yourself/);
      // The steps must install where the hook looks (~/.claude/skills/gstack).
      assert.match(permissionDecisionReason, /git clone --depth 1 https:\/\/github\.com\/garrytan\/gstack\.git ~\/\.claude\/skills\/gstack\n/);
      assert.match(permissionDecisionReason, /cd ~\/\.claude\/skills\/gstack && \.\/setup --team\n/);
    }
  }

  // The launcher failing to run must not look like allow (0) or deny (2):
  // onFailure "block" turns these into a block on Claude Code 2.1.295+.
  r = run({ CLAUDE_PROJECT_DIR: path.join(tmp, 'missing') });
  assert.ok(r.status > 0 && r.status !== 2, 'missing hook file exits neither 0 nor 2');
  r = run({ PATH: '/nonexistent' });
  assert.ok(r.status > 0 && r.status !== 2, 'node off PATH exits neither 0 nor 2');
  console.log('check-gstack smoke: ok');
} finally {
  fs.rmSync(tmp, { recursive: true, force: true });
}
