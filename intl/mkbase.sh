#!/bin/sh
# Regenerate the the reference pot
lupdate ../fuel.pro -ts en_US.ts -source-language en.US -target-language en.US
#lconvert en_US.ts -o en_US.pot

