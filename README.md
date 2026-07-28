# Parallel Image Processor

A small C++ project for applying image filters (e.g. Gaussian) to images using parallel processing techniques.

## Overview

- Implements image filters in `src/` and image I/O in `include/`/`stb/`.
- Example filters: Gaussian blur (tests in `test/`).
- Input images are under `images/input/`; processed images are written to `images/output/`.

## Requirements

- CMake (>= 3.10)
- A C++17-capable compiler (GCC/Clang)
- Ninja (optional, recommended for fast builds)
- OpenCV with the `core` and `imgproc` components installed only when building the test suite
- GoogleTest is fetched automatically by CMake when `BUILD_TESTS=ON`

## Build

Run these commands from the repository root:

```bash
mkdir -p build
cd build
cmake .. -G Ninja -DBUILD_TESTS=OFF    # engine only, no OpenCV or GoogleTest required
cmake --build .
```

To build the test suite as well, omit `-DBUILD_TESTS=OFF` and make sure OpenCV is installed. GoogleTest will be fetched automatically during configuration:

```bash
cmake .. -G Ninja
cmake --build .
```

The build will produce executables and tests under the `build/` directory.

## Run

Run the built executable (adjust the path/name depending on your generator):

```bash
# example (may vary):
./build/engine/engine     # or ./build/src/main
```

Place input images in `images/input/` and check `images/output/` for results.

## Engine Usage

```
engine --input <file> --output <file> --filter <filter> [options]
```

### Required arguments

| Flag                | Description                      |
| ------------------- | -------------------------------- |
| `--input <file>`    | Input image path                 |
| `--output <file>`   | Output image path                |
| `--filter <filter>` | `grayscale` \| `blur` \| `sobel` |

### Filter-specific options

**Blur:**

| Flag                  | Description                        |
| --------------------- | ---------------------------------- |
| `--kernel_size <int>` | Kernel size (odd positive integer) |
| `--sigmaX <float>`    | Sigma in X direction               |
| `--sigmaY <float>`    | Sigma in Y direction               |
| `--border <mode>`     | Border handling mode               |

**Sobel:**

| Flag                  | Description           |
| --------------------- | --------------------- |
| `--dx <int>`          | Derivative order in X |
| `--dy <int>`          | Derivative order in Y |
| `--kernel_size <int>` | Kernel size           |
| `--scale <double>`    | Scale factor          |
| `--delta <double>`    | Delta value           |
| `--border <mode>`     | Border handling mode  |

**Border modes:** `clamp` \| `reflect` \| `mirror` \| `wrap` \| `constant`

### Other options

| Flag            | Description                                                               |
| --------------- | ------------------------------------------------------------------------- |
| `--threads <n>` | Number of worker threads                                                  |
| `--benchmark`   | Enable benchmarking mode                                                  |
| `--runs <n>`    | Number of timed iterations in benchmark mode (default: 10)                |
| `--csv <file>`  | Append benchmark results to a CSV file (default: `benchmark_results.csv`) |
| `-h`, `--help`  | Show the help message                                                     |

### Examples

```bash
# Grayscale
./build/engine/engine --input images/input/in.png --output images/output/out.png --filter grayscale

# Gaussian blur
./build/engine/engine --input images/input/in.png --output images/output/out.png \
  --filter blur --kernel_size 5 --sigmaX 1.5 --sigmaY 1.5

# Sobel edge detection
./build/engine/engine --input images/input/in.png --output images/output/out.png \
  --filter sobel --dx 1 --dy 0 --kernel_size 3

# Benchmark blur over 4 threads for 20 runs, appending results to a CSV
./build/engine/engine --input images/input/in.png --output images/output/out.png \
  --filter blur --kernel_size 5 --sigmaX 1.5 --benchmark --runs 20 --threads 4 --csv results.csv
```

In benchmark mode, the engine runs the selected filter `--runs` times, reports min/median/mean/max timings in milliseconds, and (if `--csv` is set) appends one row per run to the CSV file — creating it with a header if it doesn't already exist.

## Tests

From the `build/` directory run:

```bash
ctest -V
```

The test binaries require OpenCV to be available in the build environment.

## Contributing

PRs and issues welcome. Please follow project coding style and add tests for new behavior.

## License

See project root for license information.
