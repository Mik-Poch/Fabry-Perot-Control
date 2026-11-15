# Optional real-camera integration frames

Place representative positive frames here as unpadded grayscale **binary PGM
(P5), maxval 255**, extension `.pgm`. These local files are ignored by Git.
`ctest -R real_frames --output-on-failure` analyzes every such frame and requires
success. The runner widens these samples to U16 before calling API v2.
When none are supplied the test reports **Skipped** (exit 77), not a
pass. No real camera frames are included in v0.1.

The runner prints measurements for manual comparison. Without independent
ground-truth annotations it verifies successful processing, not metrological
accuracy. Use negative/unsupported frames in dedicated tests with their expected
status, rather than this positive-example directory. PNG/TIFF conversion is a
development step; no image codecs are linked into the DLL.
