
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
