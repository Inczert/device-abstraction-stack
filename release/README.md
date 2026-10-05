# DAS release process

A DAS release is permitted only when CI and the physical STM32H755 campaign both pass on the exact commit that will be tagged.

## v0.1.0 flow

1. Finish release preparation on `develop`.
2. Require CI to pass on the exact `develop` release-candidate SHA.
3. Run the complete physical campaign on that exact SHA:

   ```bash
   ./scripts/stm32h755_test_campaign.sh \
       /path/to/STM32CubeH7 \
       --clean
   ```

4. Acceptance requires exactly:

   ```text
   PASS: 40
   FAIL: 0
   Exit code: 0
   ```

   The campaign also writes `<archive>.sha256`.

5. Do not create another commit after the successful campaign.
6. Fast-forward `main` to the qualified `develop` SHA. Do not create a merge commit, because that would make the tag point at a different, untested commit.
7. Confirm `main` and `develop` both equal the qualified SHA.
8. From GitHub Actions, run **Release DAS** on `main` and provide:
   - qualified commit SHA;
   - campaign archive filename;
   - campaign archive SHA-256;
   - PASS count;
   - FAIL count;
   - campaign exit code.
9. The release workflow verifies:
   - it is running from `main`;
   - `main` equals the qualified commit;
   - project/release versions match;
   - the expected HIL counts are supplied;
   - the SHA-256 is syntactically valid;
   - normal CI has a successful run for the same commit;
   - the tag does not already exist.
10. Actions builds target-specific Release packages for CM7 and CM4 from that exact commit, validates the installed package contract, adds release provenance, generates `SHA256SUMS`, creates the annotated tag and publishes the GitHub Release.

## Why the release Action does not run HIL

GitHub-hosted runners do not own the physical NUCLEO fixture. CI remains structural/build/package validation. The local hardware campaign remains the authority for electrical/runtime qualification, and its evidence is an explicit prerequisite to publishing.

## No post-qualification edits

Even documentation-only commits change the release SHA. Finish release notes and metadata before the final hardware campaign. If anything is committed after qualification, rerun the campaign before tagging.
