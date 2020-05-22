#!/bin/sh
# Verify that the various build systems produce identical results
set -ex

# Original build system
rm -rf btmp1 pkgtmp1
mkdir btmp1 pkgtmp1
export DESTDIR=$(pwd)/pkgtmp1
cd btmp1
  ../configure
  make
  make install
cd ..

# New build system
rm -rf btmp2 pkgtmp2
mkdir btmp2 pkgtmp2
export DESTDIR=$(pwd)/pkgtmp2
cd btmp2
  cmake -G Ninja ..
  ninja
  ninja install
cd ..

if diff -Nur pkgtmp1 pkgtmp2
then
  echo pkgcheck-cmake-bits-identical PASS
else
  echo pkgcheck-cmake-bits-identical FAIL
  exit 1
fi
