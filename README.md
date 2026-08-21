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

## Requirements

- Qt 6: `qt6-base`, `qt6-declarative`
- `xdg-desktop-portal` and a portal backend

The iA Writer Mono font is bundled under the SIL Open Font License 1.1; see `fonts/OFL.txt`.
