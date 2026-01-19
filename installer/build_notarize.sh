#!/bin/bash

PACKAGE_NAME="EsattoArco_X2.pkg"
BUNDLE_NAME="org.rti-zone.EsattoArcoX2"

if [ ! -z "$app_id_signature" ]; then
    codesign -f -s "$app_id_signature" --verbose ../build/Release/libEsatto.dylib
    codesign -f -s "$app_id_signature" --verbose ../build/Release/libArco.dylib
fi

mkdir -p ROOT/tmp/EsattoArco_X2/
cp "../PrimaLuceLab.png" ROOT/tmp/EsattoArco_X2/
cp "../Esatto.ui" ROOT/tmp/EsattoArco_X2/
cp "../Arco.ui" ROOT/tmp/EsattoArco_X2/
cp "../rotatorlist Arco.txt" ROOT/tmp/EsattoArco_X2/
cp "../focuserlist Esatto.txt" ROOT/tmp/EsattoArco_X2/
cp "../build/Release/libEsatto.dylib" ROOT/tmp/EsattoArco_X2/
cp "../build/Release/libArco.dylib" ROOT/tmp/EsattoArco_X2/

if [ ! -z "$installer_signature" ]; then
	# signed package using env variable installer_signature
	pkgbuild --root ROOT --identifier $BUNDLE_NAME --sign "$installer_signature" --scripts Scripts --version 1.0 $PACKAGE_NAME
	pkgutil --check-signature ./${PACKAGE_NAME}
	xcrun notarytool submit "$PACKAGE_NAME" --keychain-profile "$AC_PROFILE" --wait
	xcrun stapler staple $PACKAGE_NAME
else
    pkgbuild --root ROOT --identifier $BUNDLE_NAME --scripts Scripts --version 1.0 $PACKAGE_NAME
fi

rm -rf ROOT
