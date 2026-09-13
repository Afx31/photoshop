# Photoshop

Simple photoshop app to modify images for social media posting.

## Build

Needs CMake 3.16+, a C++20 compiler, pkg-config, Qt 6 (Widgets/Gui), libheif, and libjxl. OpenMP is optional.

On Arch:

```sh
sudo pacman -S --needed cmake ninja gcc pkgconf qt6-base libheif libjxl
```

Then:

```sh
git clone <repo-url>
cd photoshop
cmake -S . -B build -G Ninja
cmake --build build
```

The binary is written to `./photo`.

## Run

```sh
./photo
./photo /path/to/image.jpg
```

Open a folder (or drop files) to adjust, crop, rotate, and save.
