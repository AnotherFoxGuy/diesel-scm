#!/bin/bash
export MARKS=marks
export DUMP=changes.fastexport
export REPO=fuel.fossil


if [ ! -f $MARKS-fossil ]; then
	touch $MARKS-fossil
fi

fossil export --git --import-marks $MARKS-fossil --export-marks $MARKS-fossil $REPO >$DUMP

if [ ! -f $MARKS-git ]; then
	touch $MARKS-git
fi

git fast-import --export-marks=$MARKS-git --import-marks=$MARKS-git <$DUMP
