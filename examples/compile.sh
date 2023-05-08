#!/bin/sh
set -e

if [ ! -e ../fltk ]; then
  git clone --depth 1 https://github.com/fltk/fltk ../fltk
fi

if [ ! -e ../build ]; then
  mkdir -p ../build
  cd ../build
  cmake ../fltk -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$PWD/usr" \
    -DFLTK_BUILD_EXAMPLES=OFF \
    -DFLTK_BUILD_TEST=OFF \
    -DOPTION_OPTIM="-O3" \
    -DOPTION_USE_GL=OFF
  make -j$(nproc)
  make install
  cd ../examples
fi

cxxflags="-Wall -Wno-deprecated-declarations -O3 -I../include -I../src -I../icons"
ldflags="-s"
fltk_cxxflags="$(../build/fltk-config --use-images --cxxflags)"
fltk_ldflags="$(../build/fltk-config --use-images --ldflags)"

set -x

g++ $fltk_cxxflags $cxxflags -o tree tree.cpp $fltk_ldflags $ldflags
g++ $cxxflags print_xdg_dirs.cpp -o print_xdg_dirs $ldflags
g++ $fltk_cxxflags $cxxflags -o listfiles_extension listfiles_extension.cpp $fltk_ldflags $ldflags
g++ $fltk_cxxflags $cxxflags -o listfiles_simple listfiles_simple.cpp $fltk_ldflags $ldflags

g++ $fltk_cxxflags $cxxflags -DDLOPEN_MAGIC=1 -o listfiles_magic_dlopen listfiles_magic.cpp $fltk_ldflags $ldflags
g++ $fltk_cxxflags $cxxflags -o fileselection fileselection.cpp $fltk_ldflags $ldflags -lmagic
g++ $fltk_cxxflags $cxxflags -o listfiles_magic listfiles_magic.cpp $fltk_ldflags $ldflags -lmagic

