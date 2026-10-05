# DAS release process

DAS keeps physical qualification manual and release mechanics automatic.

The root `VERSION` file is the single source of truth for the project version, CMake package version and release tag:

```text
VERSION = 0.1.0
tag     = v0.1.0
```

Changing `VERSION` on `develop` does **not** create a tag. Tagging automation runs only after a successful CI run caused by a push to `main`.

## Human release gate

Before a release candidate is merged or pushed to `main`:

1. finish code, documentation, changelog and release notes on `develop`;
2. set the intended release number in `VERSION`;
3. require normal `develop` CI to pass;
4. run the complete physical campaign:

   ```bash
   ./scripts/stm32h755_test_campaign.sh \
       /path/to/STM32CubeH7 \
       --clean
   ```

5. require exactly:

   ```text
   PASS: 40
   FAIL: 0
   Exit code: 0
   ```

6. keep the generated evidence archive and its `.sha256`;
7. merge/push that qualified candidate to `main` without changing the tested source content.

GitHub-hosted runners do not own the NUCLEO fixture, so automation cannot honestly perform or infer this HIL step. The human decision to promote the tested candidate to `main` is the explicit release authorization.

If a conflict resolution or any other edit changes release source content after the campaign, rerun the full campaign before promotion.

## Automatic path after main

Normal CI already runs for every push to `main`.

After that CI run succeeds, `.github/workflows/tag-release.yml`:

1. verifies the successful CI run belongs to the current `main` commit;
2. reads `VERSION`;
3. validates the matching changelog entry and `release/v<VERSION>.md`;
4. derives the tag as `v<VERSION>`;
5. creates and pushes the annotated tag only if that version has not already been tagged;
6. invokes the reusable packaging workflow.

If the same `VERSION` is already tagged, later `main` CI runs do nothing. A new release therefore requires an intentional version change on `develop`.

## Packaging flow

`.github/workflows/release.yml` is called automatically by the tag workflow. It validates that the tag points at the expected source revision, then builds target-specific Release packages for CM7 and CM4.

For v0.1.0 the published assets are:

```text
das-0.1.0-stm32h755-cm7.tar.gz
das-0.1.0-stm32h755-cm4.tar.gz
libdas-0.1.0-stm32h755-cm7.a
libdas-0.1.0-stm32h755-cm4.a
SHA256SUMS
```

Each full package contains the static library, public headers, CMake package files, selected linker script, Apache-2.0 license, ABI/compiler build metadata and release revision metadata.

The GitHub tag/source archive remains authoritative. The standalone `.a` files are convenience assets for consumers that do not need the full CMake installation tree.

## Automation retry behavior

GitHub does not start a second workflow merely because a workflow using `GITHUB_TOKEN` pushed a tag. The tag workflow therefore calls the packaging workflow directly after creating the tag.

If tagging succeeds but packaging fails before the GitHub Release is created, rerunning CI for that same `main` commit lets the tag workflow detect the existing tag and missing release and retry packaging.

## Version lifecycle

For the next release:

1. change `VERSION` on `develop`;
2. add/update the corresponding changelog and `release/v<VERSION>.md`;
3. complete normal development and CI;
4. perform the manual hardware release gate;
5. promote to `main`.

No manual tag command and no manual release-dispatch form are part of the normal flow.
