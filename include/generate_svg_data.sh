#!/bin/sh -e

printf "#ifndef SVG_DATA_H\n#define SVG_DATA_H\n\n" > svg_data.h

for f in ../icons/*.svg ; do
  var=$(basename "$f" | tr 'a-z' 'A-Z' | tr -d '\n' | tr -c 'A-Z0-9_' '_')
  echo "#define ${var}_DATA \\\\" >> svg_data.h
  scour -i $f --no-renderer-workaround --strip-xml-prolog --remove-descriptive-elements --enable-comment-stripping \
    --disable-embed-rasters --indent=none --strip-xml-space --enable-id-stripping --shorten-ids \
	| sed "s|\"|\'|g; s|^|  \"|; s|$|\" \\\\|;" >> svg_data.h
  printf "  /**/\n\n" >> svg_data.h
done

echo "#endif  // SVG_DATA_H" >> svg_data.h
