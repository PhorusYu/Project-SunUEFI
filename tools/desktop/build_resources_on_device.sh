#!/bin/sh
# Build Resources natively on a Debian arm64 device (no emulation).
#
# Meson normally injects these constants; a plain `cargo build` needs them passed
# explicitly or the app panics at startup (LIBEXECDIR must point at the
# /usr/libexec/resources subdirectory, and RESOURCES_FILE at the installed
# gresource, otherwise the process-helper spawn and the UI resource load fail).
set -eu
src=${1:-.}
: "${RESOURCES_FILE:=/usr/share/resources/resources.gresource}"
: "${LIBEXECDIR:=/usr/libexec/resources}"
export APP_ID=org.gnome.Resources
export GETTEXT_PACKAGE=resources
export LOCALEDIR=/usr/share/locale
export DEVLOPMENT_BUILD=false
export VERSION="${VERSION:-51.0.0}"
export RESOURCES_FILE LIBEXECDIR
exec cargo build --release --manifest-path "$src/Cargo.toml" --jobs "$(nproc)"
