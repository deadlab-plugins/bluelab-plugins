#!/bin/bash

BASEDIR=$(dirname $0)
cd $BASEDIR/..

DEMO=0
if [ "$1" == "demo" ]
then
DEMO=1
fi

VERSION=`echo | grep PLUG_VERSION_HEX config.h`
VERSION=${VERSION//\#define PLUG_VERSION_HEX }
VERSION=${VERSION//\'}
MAJOR_VERSION=$(($VERSION & 0xFFFF0000))
MAJOR_VERSION=$(($MAJOR_VERSION >> 16)) 
MINOR_VERSION=$(($VERSION & 0x0000FF00))
MINOR_VERSION=$(($MINOR_VERSION >> 8)) 
BUG_FIX=$(($VERSION & 0x000000FF))

FULL_VERSION=$MAJOR_VERSION"."$MINOR_VERSION"."$BUG_FIX

PLUGIN_NAME=`echo | grep BUNDLE_NAME config.h`
PLUGIN_NAME=${PLUGIN_NAME//\#define BUNDLE_NAME }
PLUGIN_NAME=${PLUGIN_NAME//\"}

ZIP_NAME=$PLUGIN_NAME-v$FULL_VERSION-linux

if [ $DEMO == 1 ]
then
    #DMG_NAME=$DMG_NAME-demo
    ZIP_NAME=$PLUGIN_NAME-v$FULL_VERSION[DEMO]-linux
fi

if [ $DEMO == 1 ]
then
  echo "making $PLUGIN_NAME version $FULL_VERSION DEMO linux distribution..."
else
  echo "making $PLUGIN_NAME version $FULL_VERSION linux distribution..."
fi

echo "building"


make -k vst2 BUILD_CONFIG=Release CMAKE_CXX_FLAGS="-DDEMO_VERSION=$DEMO"
make -k vst3 BUILD_CONFIG=Release CMAKE_CXX_FLAGS="-DDEMO_VERSION=$DEMO"

echo "making installer"

mkdir -p installer/$ZIP_NAME

cp -R build-linux/$PLUGIN_NAME.vst2 installer/$ZIP_NAME
cp -R build-linux/$PLUGIN_NAME.vst3 installer/$ZIP_NAME
cp -R manual/${PLUGIN_NAME}_manual-EN.pdf installer/$ZIP_NAME
cp -R installer/license.rtf installer/$ZIP_NAME
cp -R installer/install-plug.sh installer/$ZIP_NAME

strip installer/$ZIP_NAME/$PLUGIN_NAME.vst2/$PLUGIN_NAME.so
strip installer/$ZIP_NAME/$PLUGIN_NAME.vst3/Contents/x86_64-linux/$PLUGIN_NAME.so

cd installer
rm $ZIP_NAME.zip
zip -r $ZIP_NAME.zip $ZIP_NAME
cd ..

echo "done"
