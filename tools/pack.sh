#!/bin/sh
TAG=`fossil tag list | grep v | sort | tail -1`
# Strip "v" from version
VERSION=${TAG:1}
echo "Packaging Diesel Version ${VERSION}"

fossil tarball $TAG diesel-$VERSION.tar.gz -name diesel-$VERSION

