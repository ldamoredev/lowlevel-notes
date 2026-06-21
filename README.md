# Low-Level Atlas

Static personal knowledge atlas for low-level & systems programming notes.

This project mirrors the architecture of the existing **AI Atlas** and **CyberSec
Atlas**: a small Python build script, local Markdown sources, bilingual output,
sticky sidebar/topbar, client-side search, SEO metadata, sitemap, robots, and static
assets. It uses its own visual identity: a "Copper & Graphite" bare-metal palette —
copper primary, oscilloscope green, dark silicon surfaces.

The throughline: from *"I can program"* to *"I understand the machine"*, with an
operating system built from scratch as the spine project.

## Structure

```text
build.py                       # Static site generator
content/en/lowlevel/           # Canonical English Markdown notes
content/es/lowlevel/           # Spanish overlays with matching paths
static/favicon.svg             # Shared favicon (SVG source)
static/apple-touch-icon.png    # iOS home-screen icon (generated)
static/assets/atlas.css        # Design system and responsive layout
static/assets/search.js        # Theme, drawer, language memory, client search
static/assets/og-image.svg     # Social card source
static/assets/og-image.png     # Social card raster (generated, 1200x630)
static/assets/icon-192.png     # PWA icon (generated)
static/assets/icon-512.png     # PWA icon (generated, also maskable)
scripts/rasterize-brand-assets.sh   # SVG -> PNG generator
.github/workflows/deploy.yml   # GitHub Pages CI (build + deploy)
site/                          # Generated output
```

Generated pages are written to `site/en/` and `site/es/`. The root `site/index.html`
is a small language chooser that redirects to the stored or browser-preferred locale.

## Build

No framework required. The build works with the Python standard library (Python
**3.12+** — `build.py` uses PEP 701 f-strings). If the optional `markdown` package is
installed it will be used; otherwise a compact built-in renderer is the fallback.

```bash
cd /Users/nicolasbottarini/projects/low-level-programming-notes
python3 build.py
```

The build refuses to clear `site/` if it loads zero notes.

## Serve Locally

Search uses `fetch`, so serve the generated folder:

```bash
python3 -m http.server 8021 --directory site
# → http://127.0.0.1:8021/en/   ·   http://127.0.0.1:8021/es/
```

## Add a Note

Create a Markdown file under `content/en/lowlevel/<branch>/`.

```markdown
---
title: My New Note
description: Short SEO/search description.
tags: [pointers, memory]
order: 2
updated: 2026-06-21
# featured: true   # optional — promotes this note to the home "Featured" card
# draft: true      # optional — keeps the note out of the build until removed
---
# My New Note

Atomic note body.
```

Use wikilinks for internal references:

```markdown
[[lowlevel/pointers-and-memory/arena-allocators|Arena allocators]]
```

Then rebuild with `python3 build.py`.

## Add a Branch

1. Add the branch metadata in `BRANCHES` inside `build.py` (label / group / accent / icon).
2. Add an optional Spanish label/summary in `BRANCHES_ES`.
3. Create `content/en/lowlevel/<branch>/index.md`.
4. Create `content/es/lowlevel/<branch>/index.md` for a translated overlay (optional).
5. Rebuild.

The home branch cards, sidebar grouping, search metadata, and page URLs all derive
from that structure. The branch's phase is its `group`, which must match a key in
`PHASES`.

## Add a Translation

Spanish is an overlay, not a separate source tree. Create the same path under
`content/es/`. If a translation is missing, the ES page still exists and shows the
English source with a translation-pending banner. See `GLOSSARY.md` for the
translation style guide and terminology.

## SEO and Deployment

The build emits canonical URLs, `hreflang` alternates (EN/ES + `x-default`), OpenGraph
/ Twitter metadata with a PNG social image, `apple-touch-icon` + PWA icons, JSON-LD,
`sitemap.xml`, `robots.txt`, and `.nojekyll`. Set `SITE_URL` and `GITHUB_URL` when
building if the repo URL differs from the defaults.

`.github/workflows/deploy.yml` builds and publishes to GitHub Pages on every push to
`main`. Brand PNGs are regenerated with `./scripts/rasterize-brand-assets.sh` after
editing the SVG sources.

## Content

The taxonomy is **10 branches across 7 phases** (machine model → C → metal → C++
optional → systems → the OS project → always-on craftsmanship). Each branch targets
**10–15 rich, source-backed atomic notes**.

See [CONTENT-PLAN.md](CONTENT-PLAN.md) for the full taxonomy, per-branch note
outlines, curated sources, and progress checklist. Notes are written **EN-first**;
Spanish overlays follow in a second pass.
