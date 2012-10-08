#!/bin/sh
# Regenerate the the reference pot
lupdate ../fuel.pro -ts en_US.ts -source-language en.US -target-language en.US
lconvert en_US.ts -o en_US.pot

# Merge reference pot file with all languages
# Requires the gettext tool msgmerge
for i in *.po
do
	echo "Merging $i"
	msgmerge --update $i en_US.pot
done


