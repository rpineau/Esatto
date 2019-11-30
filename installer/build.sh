#!/bin/bash

mkdir -p ROOT/tmp/Esatto_X2/
cp "../Esatto.ui" ROOT/tmp/Esatto_X2/
cp "../PrimaLuceLab.png" ROOT/tmp/Esatto_X2/
cp "../focuserlist Esatto.txt" ROOT/tmp/Esatto_X2/
cp "../build/Release/libEsatto.dylib" ROOT/tmp/Esatto_X2/

PACKAGE_NAME="Esatto_X2.pkg"
BUNDLE_NAME="org.rti-zone.EsattoX2"

if [ ! -z "$installer_signature" ]; then
	# signed package using env variable installer_signature
	pkgbuild --root ROOT --identifier $BUNDLE_NAME --sign "$installer_signature" --scripts Scripts --version 1.0 $PACKAGE_NAME
	pkgutil --check-signature ./${PACKAGE_NAME}
else
	pkgbuild --root ROOT --identifier $BUNDLE_NAME --scripts Scripts --version 1.0 $PACKAGE_NAME
fi

rm -rf ROOT
