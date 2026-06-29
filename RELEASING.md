# Releasing

This document describes the maintainer flow for publishing a release on GitHub.

## 1) Pre-release checks

1. Confirm simulator build works locally.
2. Confirm core demo sounds still behave as expected.
3. Update CHANGELOG.md under [Unreleased].
4. Decide release version using SemVer.

## 2) Cut the release commit

1. Move release notes from [Unreleased] to a new dated section (for example, [1.0.1] - 2026-06-28).
2. Commit release metadata and docs updates.

Example commands:

git add .
git commit -m "chore(release): v1.0.1"

## 3) Tag and push

git tag -a v1.0.1 -m "Release v1.0.1"
git push origin main
git push origin v1.0.1

## 4) Publish GitHub Release

1. Open GitHub Releases.
2. Create a new release from tag v1.0.1.
3. Title: v1.0.1.
4. Copy highlights from CHANGELOG.md.
5. Publish release.

## 5) Post-release

1. Add fresh [Unreleased] notes section at the top of CHANGELOG.md.
2. Triage newly opened issues and feedback.
