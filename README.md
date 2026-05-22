# turmeric-spices

Official monorepo of first-party spices for the Turmeric ecosystem.

## Spices

| Spice | Description | Tier |
|-------|-------------|------|
| `tur-test` | Testing framework utilities | 1 (pure Turmeric) |
| `tur-math` | 2D/3D vector and matrix math | 1 (pure Turmeric) |

## Usage

Add a spice to your project using `tur add` with `--subdir`:

```
tur add https://github.com/turmeric-spice/turmeric-spices \
  --ref test-v0.1.0 --subdir spices/test --name test
```

See each spice's `build.tur` for the full export list.
