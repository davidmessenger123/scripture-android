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
    arm64-v8a) TARGET=android-arm64; MACHINE=AArch64 ;;
    armeabi-v7a) TARGET=android-arm; MACHINE=ARM ;;
    x86_64) TARGET=android-x86_64; MACHINE=X86-64 ;;
    x86) TARGET=android-x86; MACHINE="Intel 80386" ;;
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
if [[ $CHECK_NDK_VERSION_ONLY -eq 1 ]]; then
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
