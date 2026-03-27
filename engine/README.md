# Praxis-CAD Engine — Day 1 Build Instructions

## Artifact CLI Quick Help

The engine now builds two binaries:

- `praxis-cad`: geometry/intent workflow
- `praxis-cadcam`: geometry/intent workflow plus artifact lifecycle commands

Artifact commands are available only in `praxis-cadcam`:

```bash
./engine/build/praxis-cadcam export machine-definition --input <pkm.json> --out <machine-definition.json> [--json]
./engine/build/praxis-cadcam export capability-profile --input <machine-definition.json> --out <capability-profile.json> [--json]
./engine/build/praxis-cadcam validate <machine-definition|capability-profile|manifest> <path> [--json]
./engine/build/praxis-cadcam inspect <machine-definition|capability-profile|manifest> <path> [--json]
./engine/build/praxis-cadcam bundle create --input <artifact_dir> --out <manifest.json> [--json]
```

Expected gate behavior:

- Running artifact commands through `praxis-cad` returns an artifact-commands-unavailable error.

## Prerequisites (WSL/Ubuntu)

Install required packages:
```bash
sudo apt update
sudo apt install -y build-essential cmake ninja-build clang libocct-dev
```

Verify OCCT installation:
```bash
find /usr/include -name "TopoDS_Shape.hxx"
# Should output: /usr/include/opencascade/TopoDS_Shape.hxx
```

## Build Steps

From the repository root:

```bash
# Create build directory
mkdir -p engine/build

# Configure with CMake
cmake -S engine -B engine/build -G Ninja -DCMAKE_BUILD_TYPE=Debug

# Build
cmake --build engine/build

# Verify executable exists
ls -lh engine/build/praxis-cad
```

## Run Day 1 Test

```bash
# Create scratch directory
mkdir -p scratch

# Run the executable
cd engine/build
./praxis-cad

# Check output
ls -lh ../../scratch/box.step
```

## Validate Output

1. Open Windows File Explorer
2. Navigate to `\\wsl$\Ubuntu\home\<username>\...\Praxis\scratch\box.step`
3. Open in FreeCAD
4. Verify you see a 100mm x 50mm x 25mm box

## If Build Fails

### OCCT not found
```bash
# Check if package is installed
dpkg -l | grep occt

# If missing, install
sudo apt install libocct-dev
```

### CMake version too old
```bash
# Check version
cmake --version

# If < 3.20, install newer version from Kitware APT
```

### Ninja not found
```bash
# Fallback to Make
cmake -S engine -B engine/build -DCMAKE_BUILD_TYPE=Debug
make -C engine/build
```

## Next Steps

Once `scratch/box.step` opens successfully in FreeCAD:
- ✓ Day 1 complete
- → Move to Day 2: Intent router skeleton
