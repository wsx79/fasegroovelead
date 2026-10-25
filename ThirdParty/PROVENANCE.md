# Source provenance
Initial implementation derives from the following upstream snapshots:
- `fasegrooveguitar`: `87961a35e92be36afcc8eacbc64d77af0771cf24`
- `fasegroovepiano`: `670c8b31b682e31372d67cdfae54b72afcbcc68a`
- `fasegroovedrums`: `555ad42ddaa30c76dfeb0bfd77a9571bc5bea987`
- `fasegroovearranger`: `d51d64569a194e1b5c4dd718170ff8ba9564673c`
- `fasegroove`: `6452cedd713187a90e2e82605b0deb84384942ac`

Guitar supplies the application shell and rhythm/section infrastructure. Lead replaces lane pitch generation, note scheduling and guide synthesis, removes the circle UI and the guitar-specific piano-ducking control, and uses a separate product ID, bundle, state, preset folder, updater repository and installers. The shared module is unmodified. Piano, Drums and Arranger were consulted for family conventions.
