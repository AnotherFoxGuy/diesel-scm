#!/bin/bash
SCRIPTDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PRJDIR=$SCRIPTDIR/..
INTLDIR=$SCRIPTDIR

echo "Converting localizations"

rm -rf $PRJDIR/rsrc/intl
mkdir $PRJDIR/rsrc/intl

for i in $INTLDIR/*.ts
do
	BASE=`basename $i .ts`

	# Convert all except the en_US which is
	# the original text in the code
	if [ "$BASE" != "en_US" ]; then
		echo "$TARGET"
		lrelease-qt5 $i -qm $PRJDIR/rsrc/intl/$BASE.qm
	fi
done

