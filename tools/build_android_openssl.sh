#!/usr/bin/env bash
set -euo pipefail
umask 022
export LC_ALL=C
export TZ=UTC

OPENSSL_VERSION=3.0.22
OPENSSL_SHA256=67ebca7e50d17383028045486653492195b83db95f8558709701bb47b5c1ef81
OPENSSL_API=28
NDK_VERSION=27.2.12479018
ABI=${1:-}
WRAPPER_PREFIX=aarch64-linux-android
TARGET_CLANG_PREFIX=aarch64-linux-android
CHECK_NDK_VERSION_ONLY=0
if [[ "$ABI" == --check-ndk-version ]]; then
  CHECK_NDK_VERSION_ONLY=1
  ABI=""
fi
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
DEST="$ROOT/android/libs/$ABI"
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

if [[ $CHECK_NDK_VERSION_ONLY -eq 0 ]]; then
  case "$ABI" in
    arm64-v8a) TARGET=android-arm64; MACHINE=AArch64; WRAPPER_PREFIX=aarch64-linux-android; TARGET_CLANG_PREFIX=aarch64-linux-android ;;
    armeabi-v7a) TARGET=android-arm; MACHINE=ARM; WRAPPER_PREFIX=arm-linux-androideabi; TARGET_CLANG_PREFIX=armv7a-linux-androideabi ;;
    x86_64) TARGET=android-x86_64; MACHINE=X86-64; WRAPPER_PREFIX=x86_64-linux-android; TARGET_CLANG_PREFIX=x86_64-linux-android ;;
    x86) TARGET=android-x86; MACHINE="Intel 80386"; WRAPPER_PREFIX=i686-linux-android; TARGET_CLANG_PREFIX=i686-linux-android ;;
    *) printf 'usage: %s {arm64-v8a|armeabi-v7a|x86_64|x86}\n' "$0" >&2; exit 2 ;;
  esac
fi

: "${ANDROID_NDK_ROOT:?ANDROID_NDK_ROOT is required}"
if [[ ! -f "$ANDROID_NDK_ROOT/source.properties" ]]; then
  printf 'NDK source.properties is missing\n' >&2
  exit 1
fi
trim_ascii_whitespace()
{
  local value=$1
  value="${value#"${value%%[![:space:]]*}"}"
  value="${value%"${value##*[![:space:]]}"}"
  printf '%s' "$value"
}

ACTUAL_NDK=$(while IFS='=' read -r key value; do
  key=$(trim_ascii_whitespace "$key")
  value=$(trim_ascii_whitespace "$value")
  if [[ $key == "Pkg.Revision" ]]; then
    printf '%s' "$value"
    break
  fi
done < "$ANDROID_NDK_ROOT/source.properties")
if [[ $ACTUAL_NDK != "$NDK_VERSION" ]]; then
  printf 'NDK %s is required; found %s\n' "$NDK_VERSION" "$ACTUAL_NDK" >&2
  exit 1
fi
case "$(uname -s):$(uname -m)" in
  Linux:x86_64) HOST_TAG=linux-x86_64 ;;
  Linux:aarch64|Linux:arm64) HOST_TAG=linux-aarch64 ;;
  Darwin:x86_64|Darwin:arm64) HOST_TAG=darwin-x86_64 ;;
  *) printf 'Unsupported NDK host: %s:%s\n' "$(uname -s)" "$(uname -m)" >&2; exit 1 ;;
esac
NDK_BIN="$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/$HOST_TAG/bin"
if [[ ! -d "$NDK_BIN" ]]; then
  printf 'NDK toolchain directory is missing: %s\n' "$NDK_BIN" >&2
  exit 1
fi
for tool in clang llvm-ar llvm-ranlib; do
  if [[ ! -x "$NDK_BIN/$tool" ]]; then
    printf 'NDK tool is missing or not executable: %s\n' "$NDK_BIN/$tool" >&2
    exit 1
  fi
done
NDK_TARGET_CLANG="$NDK_BIN/${TARGET_CLANG_PREFIX}${OPENSSL_API}-clang"
NDK_TARGET_CLANGXX="$NDK_BIN/${TARGET_CLANG_PREFIX}${OPENSSL_API}-clang++"
if [[ ! -x "$NDK_TARGET_CLANG" || ! -x "$NDK_TARGET_CLANGXX" ]]; then
  printf 'NDK target compiler is missing or not executable: %s\n' "$NDK_TARGET_CLANG" >&2
  printf 'NDK target compiler is missing or not executable: %s\n' "$NDK_TARGET_CLANGXX" >&2
  exit 1
fi
NDK_WRAPPER_DIR="$WORK/ndk-wrappers"
NDK_WRAPPER_CC="$NDK_WRAPPER_DIR/${WRAPPER_PREFIX}-gcc"
NDK_WRAPPER_CXX="$NDK_WRAPPER_DIR/${WRAPPER_PREFIX}-g++"
mkdir -p "$NDK_WRAPPER_DIR"
ln -s "$NDK_TARGET_CLANG" "$NDK_WRAPPER_CC"
ln -s "$NDK_TARGET_CLANGXX" "$NDK_WRAPPER_CXX"
wrapper_paths=("$NDK_WRAPPER_CC" "$NDK_WRAPPER_CXX")
wrapper_targets=("$NDK_TARGET_CLANG" "$NDK_TARGET_CLANGXX")
for index in "${!wrapper_paths[@]}"; do
  wrapper_path=${wrapper_paths[$index]}
  wrapper_target=${wrapper_targets[$index]}
  if [[ ! -L "$wrapper_path" || ! -x "$wrapper_path" ]]; then
    printf 'NDK wrapper is missing or not executable: %s\n' "$wrapper_path" >&2
    exit 1
  fi
  if [[ "$(readlink "$wrapper_path")" != "$wrapper_target" ]]; then
    printf 'NDK wrapper has an unexpected target: %s\n' "$wrapper_path" >&2
    exit 1
  fi
done
path_entries=()
IFS=: read -r -a path_entries <<< "${PATH:-}" || true
sanitized_path=""
for path_entry in "${path_entries[@]}"; do
  [[ -n "$path_entry" && "$path_entry" != "." && "$path_entry" != "$NDK_WRAPPER_DIR" && "$path_entry" != "$NDK_BIN" ]] || continue
  sanitized_path="${sanitized_path:+$sanitized_path:}$path_entry"
done
export PATH="$NDK_WRAPPER_DIR:$NDK_BIN${sanitized_path:+:$sanitized_path}"
if [[ $CHECK_NDK_VERSION_ONLY -eq 1 ]]; then
  resolved_wrapper_cc=$(command -v "${WRAPPER_PREFIX}-gcc" || true)
  resolved_wrapper_cxx=$(command -v "${WRAPPER_PREFIX}-g++" || true)
  if [[ "$resolved_wrapper_cc" != "$NDK_WRAPPER_CC" || "$resolved_wrapper_cxx" != "$NDK_WRAPPER_CXX" ]]; then
    printf 'NDK wrappers were not resolved from %s\n' "$NDK_WRAPPER_DIR" >&2
    exit 1
  fi
  if [[ "$(readlink "$resolved_wrapper_cc")" != "$NDK_TARGET_CLANG" || "$(readlink "$resolved_wrapper_cxx")" != "$NDK_TARGET_CLANGXX" ]]; then
    printf 'NDK wrapper targets are invalid\n' >&2
    exit 1
  fi
  printf '%s\n' "$resolved_wrapper_cc"
  printf '%s\n' "$resolved_wrapper_cxx"
  printf '%s\n' "$NDK_TARGET_CLANG"
  printf '%s\n' "$NDK_TARGET_CLANGXX"
  exit 0
fi
for tool in curl sha256sum tar make patchelf readelf strings nproc; do
  command -v "$tool" >/dev/null || { printf '%s is required\n' "$tool" >&2; exit 1; }
done

ARCHIVE="$WORK/openssl-$OPENSSL_VERSION.tar.gz"
SOURCE="$WORK/openssl-$OPENSSL_VERSION"
curl --proto '=https' --tlsv1.2 --fail --location --retry 3 --output "$ARCHIVE" "https://github.com/openssl/openssl/releases/download/openssl-$OPENSSL_VERSION/openssl-$OPENSSL_VERSION.tar.gz"
printf '%s  %s\n' "$OPENSSL_SHA256" "$ARCHIVE" | sha256sum -c -
tar -xzf "$ARCHIVE" -C "$WORK"
cd "$SOURCE"
./Configure "$TARGET" -D__ANDROID_API__="$OPENSSL_API" shared no-asm no-tests
make -j"${JOBS:-$(nproc)}" SHLIB_VERSION_NUMBER= build_libs
patchelf --set-soname libcrypto_3.so libcrypto.so
patchelf --set-soname libssl_3.so libssl.so
patchelf --replace-needed libcrypto.so libcrypto_3.so libssl.so
readelf -h libcrypto.so | grep -F "Machine:" | grep -F "$MACHINE" >/dev/null
readelf -h libssl.so | grep -F "Machine:" | grep -F "$MACHINE" >/dev/null
readelf -d libcrypto.so | grep -F "[libcrypto_3.so]" >/dev/null
readelf -d libssl.so | grep -F "[libssl_3.so]" >/dev/null
readelf -d libssl.so | grep -F "[libcrypto_3.so]" >/dev/null
if readelf -d libssl.so | grep -F "[libcrypto.so]" >/dev/null; then
  printf 'libssl.so retained an unversioned libcrypto dependency\n' >&2
  exit 1
fi
strings -a libcrypto.so | grep -F "OpenSSL $OPENSSL_VERSION" >/dev/null
strings -a libssl.so | grep -F "OpenSSL $OPENSSL_VERSION" >/dev/null
install -d "$DEST"
install -m 0644 libcrypto.so "$DEST/libcrypto_3.so"
install -m 0644 libssl.so "$DEST/libssl_3.so"
printf '%s\n' "$DEST"
