#!/bin/zsh

# Requires saxon9ee and phantomjs
# saxon HE (minimum version 9)
# it needs to be alias before running the script. Something like:
# alias saxon="java -jar /Applications/oxygen/lib/saxon9ee.jar"

if [ -f ~/.zshrc ] ; then
	# Assuming you aliased saxon in .zshrc
	source ~/.zshrc 
elif [ -f ~/.bashrc ] ; then
	# Assuming you aliased saxon in .bashrc
	source ~/.bashrc
else
	source ~/.bash_profile
fi
setopt aliases

#shopt -s expand_aliases

if [ ! -e tmp ]; then
    mkdir tmp
fi

if ! command -v phantomjs >/dev/null 2>&1 ; then
	echo >&2 "Phantomjs is required.  Aborting.";
	exit 1;
fi

echo "Generating metadata for Leipzig..."
fontforge generate_svg.py Leipzig-5.2.sfd
fontforge generate_font_metadata.py Leipzig-5.2.sfd

echo "Generating C++ header file ..."
saxon supported.xsl generate-h.xsl > ../include/vrv/smufl.h

echo "Generating Bravura files ..."
saxon Bravura.svg extract-glyphs.xsl > tmp/Bravura-bounding-boxes.svg
phantomjs generate-bbox.js tmp/Bravura-bounding-boxes.svg ../data/Bravura.xml json/bravura_metadata.json

echo "Generating Leipzig files ..."
saxon Leipzig.svg extract-glyphs.xsl > tmp/Leipzig-bounding-boxes.svg
phantomjs generate-bbox.js tmp/Leipzig-bounding-boxes.svg ../data/Leipzig.xml json/leipzig-5.2_metadata.json

echo "Generating Gootville files ..."
saxon Gootville.svg extract-glyphs.xsl > tmp/Gootville-bounding-boxes.svg
phantomjs generate-bbox.js tmp/Gootville-bounding-boxes.svg ../data/Gootville.xml json/gootville-1.2_metadata.json

echo "Generating Petaluma files ..."
saxon Petaluma.svg extract-glyphs.xsl > tmp/Petaluma-bounding-boxes.svg
phantomjs generate-bbox.js tmp/Petaluma-bounding-boxes.svg ../data/Petaluma.xml json/petaluma_metadata.json

echo "Generating Machaut files ..."
fontforge generate_svg.py Machaut.sfd
fontforge generate_font_metadata.py Machaut.sfd
saxon Machaut.svg extract-glyphs.xsl > tmp/Machaut-bounding-boxes.svg
phantomjs generate-bbox.js tmp/Machaut-bounding-boxes.svg ../data/Machaut.xml json/machaut_metadata.json

echo "Generating Leland files ..."
saxon Leland.svg extract-glyphs.xsl > tmp/Leland-bounding-boxes.svg
phantomjs generate-bbox.js tmp/Leland-bounding-boxes.svg ../data/Leland.xml json/leland_metadata.json

echo "Done!"
