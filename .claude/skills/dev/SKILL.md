---
name: dev
description: >
  KartUp dev workflow: after any code change, updates CLAUDE.md to reflect the change,
  creates a feature branch, commits all modified files, and pushes to GitHub.
  Use when user says "/dev", "commita", "sobe no github", "cria branch", or after finishing
  a feature/fix implementation in this project.
---

You are executing the KartUp dev workflow. Run every step in order. No skipping.

## Context

Project: KartUp firmware (ESP32 CYD lap timer). Repo: `https://github.com/gustavmattos/kartup.git`. Working dir: the `.KARTUP.PROLAPTIMER` folder. Main docs: `CLAUDE.md` (technical reference) and `README.md` (user-facing).

## Step 1 — Understand changes

Run in parallel:
- `git diff HEAD` — see all unstaged changes
- `git status --short` — see staged/untracked files

Read `CLAUDE.md` to understand current documented state.

## Step 2 — Update CLAUDE.md

Based on what changed in the code, update `CLAUDE.md` to reflect the new state. Rules:
- If new feature added → add/update relevant section (Interface, Modos de Operação, Lógica de Volta, etc.)
- If bug fixed → add entry to "Histórico de Problemas Resolvidos" with pattern: `**Problema** — causa. Solução: fix aplicado.`
- If existing behavior changed → update the section that describes it
- Never add a full changelog block — integrate the info into the right section
- Keep technical accuracy: exact pin numbers, exact function names, exact variable names

## Step 3 — Generate branch name

From the changes, derive a branch name:
- Format: `feat/<short-description>` or `fix/<short-description>`
- Kebab-case, max 40 chars, no special chars
- Examples: `feat/lap-overlay-3s`, `fix/gps-baud-detect`, `feat/wifi-ap-download`

Check if branch already exists: `git branch --list <branch-name>`
- If exists, append `-2`, `-3`, etc.

## Step 4 — Create branch and commit

Run sequentially:
```
git checkout -b <branch-name>
git add src/main.cpp CLAUDE.md README.md platformio.ini
```

Only add files that are actually modified. Check `git status` output from Step 1. Never add: `.env`, `*.local*`, unrelated files, binary files not already tracked.

For untracked files in `src/` that are new features: add them too.

Generate commit message (Conventional Commits, ≤50 chars subject):
```
git commit -m "$(cat <<'EOF'
<type>(<scope>): <summary>

<body if needed — why, not what>

Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>
EOF
)"
```

Types: `feat`, `fix`, `refactor`, `perf`, `docs`

## Step 5 — Push to GitHub

```
git push -u origin <branch-name>
```

## Step 6 — Report

Tell user:
- Branch criada: `<branch-name>`
- Arquivos commitados: list
- O que foi atualizado no CLAUDE.md (1-2 linhas)
- URL do push: `https://github.com/gustavmattos/kartup/tree/<branch-name>`

## Error handling

- If `git push` fails due to auth: tell user to run `! git push -u origin <branch-name>` themselves
- If no changes detected (`git status` clean): tell user "Nenhuma alteração detectada. Nada a commitar."
- Never force-push, never amend published commits, never `--no-verify`
