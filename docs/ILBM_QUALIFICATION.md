# External ILBM qualification

Status: **not yet qualified against independent Amiga-produced samples**. Synthetic tests in CI are necessary but insufficient.

## Candidate source

The public [IFFshow example directory](https://github.com/mdoege/IFFshow/tree/master/demo_images) contains several `.iff` files. **Do not assume artwork inherits the repository's code license.** Confirm provenance and redistribution rights for each candidate before vendoring it. It is fine to download a file locally for inspection where permitted, without committing it.

## Reproducible local workflow

1. Identify a specific file and immutable upstream commit; record its source URL, producer (if known), license/permission, and download date.
2. Download it outside the Git working tree. Do not add external binary samples to this repository by default.
3. Run `make` followed by `python3 tools/qualify_external_ilbm.py /absolute/path/to/sample.iff --cli ./amiterrain`.
4. Save the JSON report with its SHA-256 digest, dimensions, bitplanes, compression, masking, CAMG and chunks; attach it to a qualification issue or test report.
5. Inspect the same file with an independent ILBM viewer/decoder and compare *pixel indices* (not RGB colours) with AmiTerrain's imported height values. For N planes, the expected mapping is `floor(index * 65535 / (2^N - 1))`.
6. Record the software that originally wrote the file, and whether that provenance is verified. A file that merely opens in an Amiga viewer is not evidence it was produced by a particular Amiga application.
7. Report pass/fail, any differences, and the exact AmiTerrain commit used. Do not promote ILBM beyond Experimental without independent cross-decoder comparisons.

## Evidence checklist

| Field | Required |
| --- | --- |
| Upstream immutable URL + commit | Yes |
| File SHA-256 | Yes |
| Originating application and version | If verifiable; otherwise unknown |
| Rights / permission to redistribute | Before any binary is committed |
| Dimensions / bitplanes / compression / CAMG | Yes |
| AmiTerrain CLI result and commit | Yes |
| Independent decoder comparison | Yes for Verified |
| Height interpretation and palette independence | Yes for Verified |

### Failure handling

A rejected file is still useful evidence: retain the JSON error report and minimal reproduction steps, then determine whether the file is malformed, an unsupported ILBM display mode, or a parser defect. Never silently classify an unsupported artwork format as valid terrain.

## External sample acquisition (without vendoring)

For the first candidate, inspect [IFFshow's `amiga_lagoon.iff`](https://github.com/mdoege/IFFshow/blob/master/demo_images/amiga_lagoon.iff). GitHub's text-only API cannot retrieve this binary through the current integration; this is an access limitation, **not** a qualification failure. The sample's creator and image redistribution rights remain unverified.

On a networked workstation, download the file to a temporary directory outside the checkout, then run:

```sh
curl --fail --location --output /tmp/amiga_lagoon.iff \
  https://raw.githubusercontent.com/mdoege/IFFshow/master/demo_images/amiga_lagoon.iff
make
python3 tools/qualify_ilbm.py /tmp/amiga_lagoon.iff
python3 tools/qualify_external_ilbm.py /tmp/amiga_lagoon.iff --cli ./amiterrain
```

**Do not** add a manifest entry until the file has actually been downloaded, its SHA-256 recorded, the originating revision pinned, and the CLI report captured. Do not mark independent comparison as passing until another decoder has been used and pixel indices compared. An artwork image may be a valid ILBM without being a meaningful terrain heightmap.

### Provenance note: amiga_lagoon.iff

IFFshow's [`image_credits.txt`](https://github.com/mdoege/IFFshow/blob/master/image_credits.txt) attributes `amiga_lagoon.iff` to **Jim Sachs**, as a demo image for the **Brilliance** painting program. This identifies the credited artwork and association, **not** the specific ILBM-writing application/version, nor a license grant from the artist. Treat the image as third-party artwork: do not vendor or redistribute it without explicit permission. A successful decode would establish compatibility with this particular ILBM bitstream only, not blanket Brilliance export compatibility.


GitHub's tree metadata identifies the candidate as Git blob [`713d45a6eee3d4d8e2ee9ed17cf8b120285d2363`](https://github.com/mdoege/IFFshow/blob/713d45a6eee3d4d8e2ee9ed17cf8b120285d2363/demo_images/amiga_lagoon.iff), size **484,242 bytes**. This is a **Git object SHA-1, not the sample's SHA-256**; do not substitute it for the digest required by the qualification manifest. Neither metadata nor artwork provenance establishes a passing import test.
