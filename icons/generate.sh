#!/bin/sh
for f in *.svg ; do
  var=$(printf "$f" | tr 'A-Z' 'a-z' | tr -c 'a-z0-9_' '_')
  echo "const char *$var =" > ${f}.h
  scour $f | sed "s|\"|\'|g; s|^|  \"|; s|$|\"|; s|</svg>\"|</svg>\";|" >> ${f}.h
done

