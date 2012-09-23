#!/bin/sh
SCRIPTDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PRJDIR=$SCRIPTDIR/..
INTLDIR=$SCRIPTDIR

echo "Converting localizations"

rm -rf $PRJDIR/rsrc/intl
mkdir $PRJDIR/rsrc/intl

for i in $INTLDIR/*.po
do
	TARGET=`basename $i .po`.qm
	echo "$TARGET"
	lconvert $i -o $PRJDIR/rsrc/intl/$TARGET
done

