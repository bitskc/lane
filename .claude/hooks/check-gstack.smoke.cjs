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

function run(env) {
  // Real node first on PATH: version-manager shims can resolve node through HOME.
  const nodePath = path.dirname(process.execPath) + path.delimiter + process.env.PATH;
  const base = { PATH: nodePath, HOME: path.join(tmp, 'home'), CLAUDE_PROJECT_DIR: repo };
  return spawnSync('/bin/sh', ['-c', handler.command], { env: { ...base, ...env }, encoding: 'utf8' });
}

try {
  mk('home');
  const binIsFile = mk('binfile');
  fs.writeFileSync(path.join(binIsFile, 'bin'), '');

  let r = run({ GSTACK_ROOT: path.dirname(mk('installed', 'bin')) });
  assert.strictEqual(r.status, 0, 'GSTACK_ROOT install allows');
  assert.strictEqual(r.stdout.trim(), '{}');

  // Installs found through HOME, with no GSTACK_ROOT.
  for (const rel of ['.claude/skills/gstack', '.codex/skills/gstack', '.gstack/repos/gstack']) {
    const home = mk('home-' + rel.replace(/\W/g, '_'));
    mk('home-' + rel.replace(/\W/g, '_'), rel, 'bin');
    r = run({ HOME: home });
    assert.strictEqual(r.status, 0, `${rel} under HOME allows`);
  }

  for (const [name, env] of [
    ['no install anywhere', {}],
    ['empty GSTACK_ROOT', { GSTACK_ROOT: '' }],
    ['bin is a file', { GSTACK_ROOT: binIsFile }],
  ]) {
    r = run(env);
    assert.strictEqual(r.status, 2, `${name} denies`);
    assert.strictEqual(JSON.parse(r.stdout).hookSpecificOutput.permissionDecision, 'deny');
    assert.match(r.stderr, /BLOCKED: gstack is not installed globally/);
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
