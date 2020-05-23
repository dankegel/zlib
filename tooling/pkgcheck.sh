#!/bin/sh
# Verify that the various build systems produce identical results on a Unixlike system
set -ex

# Tell GNU's ld etc. to use Jan 1 1970 when embedding timestamps
export SOURCE_DATE_EPOCH=0
# Tell Apple's ar etc. to use zero timestamps
export ZERO_AR_DATE=1

# Original build system
rm -rf btmp1 pkgtmp1
mkdir btmp1 pkgtmp1
export DESTDIR=$(pwd)/pkgtmp1
cd btmp1
  case $(uname) in
  Darwin)
    # Tell configure which compiler etc. to use to match cmake
    # Tell configure to pad the executable the same way cmake will
    CC="$(xcrun --find gcc || echo gcc)" \
    CFLAGS="-DNDEBUG -O3 -isysroot $(xcrun --show-sdk-path)" \
    LDFLAGS="-Wl,-headerpad_max_install_names" \
    ../configure
    ;;
  FreeBSD)
    # 1) tell configure to tell ar to be deterministic?
    # 2) configure fails with cc, so use clang or gcc
    # 3) Force same CC for configure and clang
    #export ARFLAGS="Drc"
    if clang --version
    then
        export CC=clang
        ../configure
    elif gcc --version
    then
        export CC=gcc
        ../configure
    else
        # will probably fail
        ../configure
    fi
    ;;
  *)
    ../configure
    ;;
  esac
  make
  make install
cd ..

# New build system
rm -rf btmp2 pkgtmp2
mkdir btmp2 pkgtmp2
export DESTDIR=$(pwd)/pkgtmp2
cd btmp2
  cmake -G Ninja ..
  ninja -v
  ninja install
cd ..

case $(uname) in
Darwin)
  # Alas, dylibs still have an embedded hash or something,
  # so nuke it.  There's probably a better way.
  dylib1=$(find pkgtmp1 -name '*.dylib*' -type f)
  dylib2=$(find pkgtmp2 -name '*.dylib*' -type f)
  dd conv=notrunc if=/dev/zero of=$dylib1 skip=1337 count=16
  dd conv=notrunc if=/dev/zero of=$dylib2 skip=1337 count=16
  ;;
FreeBSD)
  # I had trouble passing -D to the ar inside CMakeLists.txt,
  # so punt and unpack the archive before comparing.
  cd pkgtmp1; ar x usr/local/lib/libz.a; rm usr/local/lib/libz.a; cd ..
  cd pkgtmp2; ar x usr/local/lib/libz.a; rm usr/local/lib/libz.a; cd ..
  ;;
esac

if diff -Nur pkgtmp1 pkgtmp2
then
  echo pkgcheck-cmake-bits-identical PASS
else
  echo pkgcheck-cmake-bits-identical FAIL
  exit 1
fi
