#!/bin/sh
SCRIPTDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PRJDIR=$SCRIPTDIR/..
INTLDIR=$SCRIPTDIR

echo "Converting localizations"

rm -rf $PRJDIR/rsrc/intl
mkdir $PRJDIR/rsrc/intl

for i in $INTLDIR/*.ts
do
	TARGET=`basename $i .ts`.qm
	echo "$TARGET"
	lrelease $i -qm $PRJDIR/rsrc/intl/$TARGET
done

