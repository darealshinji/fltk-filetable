#!/bin/sh
set -e

cxxflags="-Wall -O0 -g -I../include -I../src -I../icons"
ldflags=""
fltk_cxxflags="$(../build/fltk-config --use-images --cxxflags)"
fltk_ldflags="$(../build/fltk-config --use-images --ldflags)"

set -x

g++ $fltk_cxxflags $cxxflags -o fileselection fileselection.cpp $fltk_ldflags $ldflags
g++ $cxxflags print_xdg_dirs.cpp -o print_xdg_dirs $ldflags
g++ $fltk_cxxflags $cxxflags -o tree tree.cpp $fltk_ldflags $ldflags
g++ $fltk_cxxflags $cxxflags -DDLOPEN_MAGIC -o listfiles_magic listfiles_magic.cpp $fltk_ldflags $ldflags #-lmagic
g++ $fltk_cxxflags $cxxflags -o listfiles_extension listfiles_extension.cpp $fltk_ldflags $ldflags
g++ $fltk_cxxflags $cxxflags -o listfiles_simple listfiles_simple.cpp $fltk_ldflags $ldflags

