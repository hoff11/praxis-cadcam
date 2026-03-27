# recipes/

**Authority: machine-definition (praxis-cadcam)**

This folder contains machine-definition source data — the parametric geometry and kinematic models that describe physical machine configurations.

## Contents

| File/Folder | Type | Description |
|---|---|---|
| `vmc/vmc_3axis.pkm` | Physical Kinematic Model | 3-axis VMC kinematic chain |
| `vmc/vmc_5x_tabletable.pkm` | Physical Kinematic Model | 5-axis table-table VMC kinematic chain |
| `vmc/vmc_v0.json` | Recipe | Parametric machine geometry for VMC v0 |
| `test_machine.pkm` | Physical Kinematic Model | Test fixture kinematic model |

## File Types

- **`.pkm`** — Physical Kinematic Model: describes the kinematic chain of a machine (axes, joints, geometry).
- **`.recipe.json`** — Parametric machine geometry recipe: input to the machine generation pipeline.

## Boundary Rules

- This data is **authored** here and **consumed** by the machine-generation pipeline.
- Compiled outputs (bundles, artifacts) go to `C:\Users\hoffm\AppData\Roaming\Praxis\bundles` — never back into this folder.
- Runtime I/O goes to `C:\dev\praxis-temp\exchange` — never into this repo.
