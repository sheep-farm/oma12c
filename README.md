# OMA12C

A reverse-Polish-notation (RPN) financial calculator inspired by the legendary HP-12C, built with Qt Quick and C++ and themed automatically for Omarchy.

## Install

Clone the repository and run the included install script:

```bash
git clone https://github.com/sheep-farm/oma12c.git
cd oma12c
bin/install
```

This builds the project, copies the `oma12c` binary to `~/.local/bin`, and adds an Omarchy/Hyprland window rule so the calculator launches floating, centered and with a fixed size.

To uninstall, run `bin/install uninstall`.

## Usage

OMA12C follows the classic 4-level RPN stack of the HP-12C:

- Type a number, then press `ENTER` to push it onto the stack.
- The next number or operation works on the top of the stack.
- Arithmetic (`+`, `−`, `×`, `÷`), powers (`y^x`), roots, reciprocals, logarithms and factorial are all postfix: `2 ENTER 3 y^x` gives `8`.

### Stack

- `ENTER` duplicates or pushes the current number.
- `x<>y` swaps X and Y.
- `R↓` rolls the stack down.
- `CLx` clears the display/X-register.
- `ON` clears everything and resets the calculator.

### Prefix keys

- `f` (gold) and `g` (blue) select the alternate functions printed above and below each key.
- `STO` stores X into a numbered or financial register; `STO + 1` performs register arithmetic.
- `RCL` recalls a register.
- `FIX` followed by `0`–`9` sets the number of decimal places.
- `SCI` / `ENG` switch scientific or engineering display.
- `BEG` / `END` switch payment-due mode.

### Financial keys

Top-row `n`, `i`, `PV`, `PMT`, `FV` store when preceded by a number, or solve for the missing value when pressed alone. Enter four known cash-flow values, then press the fifth key to solve.

## Configuration

OMA12C reads display preferences from `~/.config/Omacom/oma12c.conf`:

```ini
[display]
decimalSeparator=comma
```

- `decimalSeparator` accepts `dot` (default) or `comma`.

## Building

### Linux

The default build remains unchanged:

```bash
bin/build
```

Or install locally:

```bash
bin/install
```

### Windows

Requires a Qt 6 Windows build (MinGW or MSVC) and a matching toolchain.

```bash
QMAKE=/path/to/qt-windows/bin/qmake bin/build-windows
```

The executable is produced in `build-windows/`.

### macOS

Requires Qt 6 for macOS and Xcode Command Line Tools.

```bash
QMAKE=/path/to/Qt/6.x.x/macos/bin/qmake bin/build-macos
```

The app bundle is produced in `build-macos/OMA12C.app`.

### Android

Requires the Android SDK, NDK, JDK 17+ and a Qt 6 for Android build.

```bash
QMAKE=$HOME/Qt/6.x.x/android_arm64_v8a/bin/qmake \
ANDROID_HOME=$HOME/Android/Sdk \
ANDROID_NDK=$HOME/Android/Sdk/ndk/xx.x.xxxxxxx \
bin/build-android
```

The APK is produced in `build-android/android-build/build/outputs/apk/`.

## Requirements

- Qt 6: `qt6-base`, `qt6-declarative` (Linux); equivalent Qt 6 packages on other platforms
- Linux: `xdg-desktop-portal` and a portal backend

The iA Writer Mono font is bundled under the SIL Open Font License 1.1; see `fonts/OFL.txt`.
