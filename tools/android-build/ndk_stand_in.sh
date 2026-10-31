#!/usr/bin/env bash
# Lets Gradle configure the :dolphin module in a container whose NDK 29 install is incomplete (a folder holding only
# .installer, 31 October): AGP then stops at "NDK at .../29.0.14206865 did not have a source.properties file", before any
# Kotlin compile. `on` puts in its place links to NDK 28.2 with a source.properties saying 29; `off` puts the original
# back. For Kotlin and resource compiles only: native code built with it would be built by 28.2.
#   tools/android-build/ndk_stand_in.sh on && ./gradlew ... ; tools/android-build/ndk_stand_in.sh off
set -euo pipefail
NDK="${ANDROID_HOME:-/home/user/android-sdk}/ndk"
REAL=29.0.14206865 BASE=28.2.13676358 KEEP="/tmp/ndk-$REAL-original"
case "${1:-}" in
  on)
    [ -e "$NDK/$REAL/source.properties" ] && [ ! -e "$KEEP" ] && { echo "NDK $REAL is whole: nothing to do"; exit 0; }
    [ -e "$KEEP" ] && { echo "already on"; exit 0; }
    mv "$NDK/$REAL" "$KEEP"
    mkdir "$NDK/$REAL"
    for f in "$NDK/$BASE"/* "$NDK/$BASE"/.[!.]*; do [ -e "$f" ] && ln -s "$f" "$NDK/$REAL/"; done
    rm "$NDK/$REAL/source.properties"
    sed "s/^Pkg.Revision.*/Pkg.Revision = $REAL/" "$NDK/$BASE/source.properties" > "$NDK/$REAL/source.properties"
    echo "on: $REAL stands in with $BASE" ;;
  off)
    [ -e "$KEEP" ] || { echo "not on"; exit 0; }
    rm -rf "${NDK:?}/$REAL"
    mv "$KEEP" "$NDK/$REAL"
    echo "off: the original $REAL is back" ;;
  *) echo "usage: $0 on|off"; exit 2 ;;
esac
