---
layout: page
title: Build the Docs
---

This page explains how to build and preview the documentation site locally.

## Prerequisites

- Ruby 3.1.x (Windows: install Ruby+Devkit)
- Bundler (`gem install bundler`)

## Quick start (Windows, PowerShell)

1. Open PowerShell and navigate to the docs directory:

   ```powershell
   cd <path>\HydroChrono\docs
   ```

2. Install dependencies:

   ```powershell
   gem install bundler
   bundle config set --local path vendor/bundle
   bundle install
   ```

3. Serve the site locally:

   ```powershell
   bundle exec jekyll serve --livereload
   ```

   The site uses `baseurl: /HydroChrono`. Open:

   - `http://localhost:4000/HydroChrono/`

   For root paths locally, you can override the baseurl:

   ```powershell
   bundle exec jekyll serve --livereload --baseurl ""
   ```

## Build only

```powershell
bundle exec jekyll build
```

The generated site will be in `_site/`.

## Containerized option

```powershell
docker run --rm -p 4000:4000 -v ${PWD}:/srv/jekyll -w /srv/jekyll jekyll/jekyll:3.9 jekyll serve --livereload
```

Then open `http://localhost:4000/HydroChrono/`.

## Troubleshooting

- Use Ruby 3.1.x for best compatibility with Jekyll 3.9.x.
- On Windows, ensure you installed the MSYS2 toolchain during Ruby+Devkit setup (`ridk install`).
- If port 4000 is in use, add `--port 4001`.
- First run fetches the remote theme; ensure internet connectivity.


<p align="center">
  <img src="https://nrel.github.io/HydroChrono/assets/img/wave_animation2.gif" alt="Wave Energy" width="80%" />
</p>