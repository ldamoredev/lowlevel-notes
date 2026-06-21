# AGENTS.md — Low-Level Atlas

Operational guide for any coding agent working in this repo. Read this first, then
`CONTENT-PLAN.md` (progress + per-branch note outlines) and `SOURCES.md` (curated
authoritative sources to cite).

## What this is

**Low-Level Atlas**: a static, framework-free, **bilingual** (EN canonical + ES
overlay) personal knowledge atlas about low-level & systems programming. Sibling of
`ai-notes` and `cibersecurity-notes` (same architecture, different visual identity —
a "Copper & Graphite" bare-metal palette). Plain-Python build, deployed to GitHub
Pages. The spine project is an **OS built from scratch (x86-64 via cross-compiler +
QEMU)**.

## Build, run, verify

```bash
python3 build.py                                   # no deps; needs Python 3.12+
python3 -m http.server 8021 --directory site       # → http://127.0.0.1:8021/en/
```

- The build prints a summary ending in `(unresolved links: N)`. **N must be 0.**
- After any content change, verify:
  ```bash
  python3 build.py
  grep -rl 'class="unresolved-link"' site/en site/es | wc -l   # must print 0
  ```
- `build.py` deletes and regenerates `site/` on every run. Never hand-edit `site/`.
- `python3 -m py_compile build.py` after editing `build.py`.

> Python note: `build.py` uses a PEP 701 f-string (backslash inside an f-string), so
> it needs **Python 3.12+**. CI pins 3.12. On stock macOS python (3.9) use a newer
> interpreter, e.g. `python3.13 build.py`.

## Repo layout

```
content/en/lowlevel/<branch>/<slug>.md   # canonical English notes (source of truth)
content/en/lowlevel/<branch>/index.md    # branch landing: links its notes + "Core sources"
content/es/lowlevel/<branch>/<slug>.md   # Spanish overlay (same path); missing → EN + banner
build.py                                 # generator; BRANCHES dict = taxonomy/labels/accents/icons
static/assets/atlas.css|search.js        # design system + client search
static/*.svg|*.png                       # brand assets (PNGs via scripts/rasterize-brand-assets.sh)
site/                                     # generated output — do not edit
CONTENT-PLAN.md                          # SOURCE OF TRUTH: progress checklist + per-branch outlines
SOURCES.md                               # curated authoritative sources per branch
GLOSSARY.md                              # ES translation style guide + terminology
```

## Taxonomy

10 branches grouped into 7 phases (see `CONTENT-PLAN.md`). Branch config
(label / group / accent / icon) lives in the `BRANCHES` dict in `build.py`; Spanish
labels in `BRANCHES_ES`. A branch's phase is its `group`, which must match a key in
`PHASES`. `build.py`'s `validate_taxonomy()` warns on drift. `modern-cpp` carries an
`"optional": True` flag — it's deferred until the C layer is solid.

## How to write a note (match the bar)

Frontmatter:
```yaml
---
title: "Note title"
description: One-line SEO/search description.
tags: [tag-a, tag-b]
order: 4              # position within the branch (index is order 0)
updated: 2026-06-21
# featured: true      # at most ONE note across the whole atlas
# draft: true         # stage WIP; excluded from build/search/sitemap
# translation: stale  # ES files only: EN counterpart was rewritten, translation pending
---
```

Body shape (density before length, **no fluff**):
- `#` H1 = the title.
- **Mental model first** — 2–4 sentences that compress the idea so it survives alone.
- **How it really works** — the mechanism, with correct detail (assembly, memory
  layout, the spec) where it matters.
- **At least one executable artifact** — C preferred (compiles with
  `gcc -Wall -Wextra`), assembly via Compiler Explorer, or a Makefile/CLI snippet.
  Must build/run as written. Prefer x86-64; add an **ARM64 appendix** when it helps.
- **Named things** — books, specs (Intel SDM, System V ABI, ELF), tools, with links.
  No "studies show".
- **Failure modes & trade-offs** — UB, portability, performance, a decision rule.
- **Pitfall / In practice** angle — what bites people in real code.
- Close with `**Connects to:**` + `[[wikilinks]]`, then `## Sources` — 3–8 primary
  links, each with one line on why it's worth reading. Never fabricate a citation;
  unverifiable claims get `> ⚠️ Unverified — needs source`.

Links: `[[lowlevel/<branch>/<slug>|Label]]` (path form, preferred) or `[[slug|Label]]`.
Anchors: `[[lowlevel/x/y#heading|Label]]`. **Never leave an unresolved link** — only
link to notes/indexes that already exist. Add new notes' `[[links]]` to the branch
`index.md` (replacing the plain-text roadmap entry).

Sources: each branch `index.md` ends with `## Core sources` listing real, authoritative
sources (full URLs) drawn from `SOURCES.md`.

## Renderer gotcha (important)

`markdown` is usually **not** pip-installed locally, so `build.py` falls back to its
built-in renderer. **Author for the fallback:** ATX headings, **flat** (non-nested)
bullet/numbered lists, tables, blockquotes, fenced code, inline code/bold/italic/
links. Avoid nested lists and exotic Markdown. (CI installs `markdown`+`pygments`, so
production is richer — but if it renders in the fallback it renders everywhere.)

## Conventions & constraints

- **EN-first.** ES overlays are a later pass. ES = **Rioplatense voseo** (usá, tenés,
  podés, hacé), technical and concise. Same path under `content/es/`. See `GLOSSARY.md`.
- **OS/asm decisions (fixed):** OS target = **x86-64**, cross-compiler + **QEMU**,
  host-agnostic. Assembly = **x86-64 primary** + **ARM64 appendix** per note.
- **One branch = 10–15 rich notes.** When a branch is done, tick it in `CONTENT-PLAN.md`.
- After each branch: build, confirm **0 unresolved links**, spot-check one page.
- **Do not commit unless the user explicitly asks.** Branch first if on `main`.
- Cross-link generously between branches — the value is the graph.

## Deploy

GitHub Pages via `.github/workflows/deploy.yml` (repo Settings → Pages → Source =
**GitHub Actions**). CI runs `python build.py` with `SITE_URL` = the Pages base URL.
`site/` does not need to be committed once CI is enabled.

## What's next

The scaffold is in place: all 10 branches have a written `index.md` (with planned-note
roadmaps), 8 phase pages, the entry notes (`start-here`, `must-know`,
`reference-registry`), and the taxonomy wired in `build.py`. **Next: write the atomic
notes**, one branch at a time, starting with `machine-model` → `c-from-the-metal` →
`pointers-and-memory`. Then the **ES overlay pass**. Build + verify 0 unresolved after
each branch.
