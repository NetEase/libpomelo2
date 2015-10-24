#!/bin/sh
echo ""
echo "Compiling cspomelo"

BASE=$(dirname $0)

$NDK_ROOT/ndk-build \
 NDK_MODULE_PATH=$BASE/../../ \
 NDK_PROJECT_PATH=$BASE \
 APP_BUILD_SCRIPT=$BASE/Android.mk \
 NDK_APPLICATION_MK=$BASE/Application.mk $*
mv $BASE/libs/armeabi/libcspomelo.so $BASE/unity/Assets/Plugins/Android

echo ""
echo "Cleaning up / removing build folders..."  #optional..
rm -rf $BASE/libs
rm -rf $BASE/obj

echo ""
echo "Done!"
