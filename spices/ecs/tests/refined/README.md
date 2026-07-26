# Refined ecs tests (RE1) -- manually verified, NOT auto-run

These tests exercise the aliveness-refined accessors (`ecs/refined-world`) and
the `frozen` + `#reads` machinery under `--enable=refined --strict-refine`. They
live in this **subdir** on purpose: `tur test tests` (the CI entry point) does
not descend into subdirectories, so these do not auto-run.

Why not auto-run: `tur test <dir>` compiles every file in one process, and
compiling multiple refined files in one process currently corrupts memory and
crashes nondeterministically -- see
`turmeric/docs/reported/refined-multi-compile-memory-corruption.md`. Each test
is correct and passes when run on its own; run one with:

    tur run  --enable=refined --strict-refine spices/ecs/tests/refined/<name>.tur   # positives
    tur check --enable=refined --strict-refine spices/ecs/tests/refined/<name>.tur   # negatives (expect TUR-W0372)

Positives (prove + run): refined-alive-frozen (42), refined-module-alive-frozen
(42), refined-loop-alive (40, the RE1(c) refined-loop demo).
Negatives (TUR-W0372 under --strict-refine): refined-alive-no-region,
refined-module-no-region, refined-loop-no-guard.

When the multi-compile corruption is fixed, run each via its own `tur run`
invocation from CI (not `tur test <dir>`), or move them back flat.
