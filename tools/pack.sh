#!/bin/sh
TAG=`fossil tag list | grep v | sort | tail -1`
# Strip "v" from version
VERSION=${TAG:1}
echo "Packaging Fuel Version ${VERSION}"

fossil tarball $TAG fuel-$VERSION.tar.gz -name fuel-$VERSION

