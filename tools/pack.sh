#!/bin/sh
VERSION=`fossil tag list | grep v | sort | tail -1`
echo "Packaging Fuel Version ${VERSION}"

fossil tarball $VERSION fuel-$VERSION.tar.gz -name fuel-$VERSION

