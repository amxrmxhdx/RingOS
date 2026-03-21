#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 2 ]]; then
  echo "Usage: $0 <source.c> <output.bin>"
  exit 1
fi

SRC="$1"
OUT="$2"
ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
SDK_DIR="$ROOT_DIR/programs/sdk"
BUILD_DIR="$ROOT_DIR/programs/.build"

mkdir -p "$BUILD_DIR"
mkdir -p "$(dirname "$OUT")"

if command -v i686-elf-gcc >/dev/null 2>&1; then
  CC=i686-elf-gcc
  LD=i686-elf-ld
  OBJCOPY=i686-elf-objcopy
elif command -v clang >/dev/null 2>&1 && command -v ld.lld >/dev/null 2>&1; then
  CC="clang --target=i686-elf"
  LD=ld.lld
  OBJCOPY=llvm-objcopy
else
  echo "Error: need i686-elf-gcc toolchain (or clang+lld+llvm-objcopy)."
  exit 1
fi

BASE_NAME="$(basename "$SRC" .c)"
CRT_OBJ="$BUILD_DIR/crt0.o"
CLIB_OBJ="$BUILD_DIR/clib.o"
APP_OBJ="$BUILD_DIR/${BASE_NAME}.o"
ELF_OUT="$BUILD_DIR/${BASE_NAME}.elf"

$CC -m32 -ffreestanding -fno-pic -fno-stack-protector -nostdlib -nostdinc \
  -I"$SDK_DIR/include" -c "$SDK_DIR/src/crt0.S" -o "$CRT_OBJ"

$CC -m32 -ffreestanding -fno-pic -fno-stack-protector -nostdlib -nostdinc \
  -I"$SDK_DIR/include" -c "$SDK_DIR/src/clib.c" -o "$CLIB_OBJ"

$CC -m32 -ffreestanding -fno-pic -fno-stack-protector -nostdlib -nostdinc \
  -I"$SDK_DIR/include" -c "$SRC" -o "$APP_OBJ"

$LD -m elf_i386 -T "$SDK_DIR/linker.ld" -nostdlib -o "$ELF_OUT" "$CRT_OBJ" "$CLIB_OBJ" "$APP_OBJ"
$OBJCOPY -O binary "$ELF_OUT" "$OUT"

echo "Built: $OUT"
