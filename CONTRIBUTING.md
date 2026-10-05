# Contributing to SkinAPI

Thank you for your interest in contributing! This guide covers how to work with the SkinAPI codebase.

## Branch Workflow

We follow a two-branch model:

- **`dev`**: Integration branch for in-progress work and testing. This is where features are developed, tested, and staged.
- **`main`**: Stable production branch, published to GitHub Pages. Merges from `dev` only after CI passes and local smoke tests confirm no regressions.

### Working on a Feature

1. **Create a feature branch** off `dev`:
   ```bash
   git checkout dev
   git pull origin dev
   git checkout -b feature/your-feature-name
   ```

2. **Make one logical change per commit**:
   - Use imperative, present-tense subject lines: "Add budget optimizer" not "Added budget optimizer"
   - Keep commits focused and reversible
   - Include a brief explanation of *why* the change was made
   - Example: `git commit -m "Cache SkinsTrack responses to reduce API calls"`

3. **Push to your branch** and open a pull request against `dev`:
   ```bash
   git push origin feature/your-feature-name
   ```

4. **Ensure CI passes**: All GitHub Actions checks (build, tests, linting) must pass before merging.

### Merging to Main

Once a feature is complete and tested on `dev`:

1. **Run a local smoke test** to confirm basic functionality works end-to-end.

2. **Create a PR from `dev` → `main`** when ready to release.

3. **CI must pass** on `main` (same checks as `dev`).

4. **Use fast-forward merge only**: `git merge --ff-only dev` to keep history clean.

5. **Do not commit `.env`**: Environment configuration is gitignored for security. Contributors must set up their own `.env` from `.env.example`.

## Commit Message Guidelines

- **Imperative mood**: "Add feature" not "Added feature" or "Adds feature"
- **Concise subject**: Under 50 characters
- **Optional body**: Explain *why*, not *what* (the diff shows what)
  ```
  Fix race condition in cache refresh

  The async HTTP call could complete while the main thread was
  reading from the cache file. Added a mutex to serialize access.
  ```

## Testing

- **C++ tests**: `ctest --test-dir build --output-on-failure`
- **Frontend tests**: `node --test tests/frontend/`
- **Manual testing**: Run `scripts/dev.bat` to start both API and frontend locally

## Code Style

- **C++**: Follow the existing style in `src/`. Use 4-space indents, keep lines under 100 characters where possible.
- **JavaScript**: Vanilla JS; no framework dependencies. Use const/let, avoid var.
- **CSS**: SCSS or plain CSS; keep selectors specific to avoid conflicts.

## Security

- **Never commit secrets**: `.env`, credentials, API keys, or tokens go in `.env` (gitignored).
- **Validate inputs**: All query parameters and POST bodies are validated in `src/validation.hpp`.
- **CORS**: Respect the `ALLOWED_ORIGIN` config to prevent cross-origin abuse.

For security vulnerabilities, see [SECURITY.md](SECURITY.md).

## Questions?

- Check [README.md](README.md) for API reference and architecture overview
- Review [SECURITY.md](SECURITY.md) for threat model and known limits
- Look at existing code for patterns and conventions
