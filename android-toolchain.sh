#!/bin/bash

export ANDROID_TOOLCHAIN_DIR=$PWD/android-toolchain
mkdir -p $ANDROID_TOOLCHAIN_DIR
$1/build/tools/make-standalone-toolchain.sh \
    --toolchain=arm-linux-androideabi-4.8 \
    --arch=arm \
    --install-dir=$ANDROID_TOOLCHAIN_DIR \
    --platform=android-9

