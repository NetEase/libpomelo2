#!/bin/bash

SSL=""
if [ -z $1 ]; then
  echo "Usage:  android-compile.sh <NDK_ROOT> [ssl]"
  exit 1
fi

if [ "$2"x == "ssl"x ]; then
  SSL=""
else
  SSL="-Dno_tls_support=true"
fi

export ANDROID_TOOLCHAIN_DIR=$PWD/android-toolchain

if [ ! -e $ANDROID_TOOLCHAIN_DIR ]; then
  mkdir -p $ANDROID_TOOLCHAIN_DIR
  $1/build/tools/make-standalone-toolchain.sh \
      --toolchain=arm-linux-androideabi-4.8 \
      --arch=arm \
      --install-dir=$ANDROID_TOOLCHAIN_DIR \
      --platform=android-9
fi

export PATH=$ANDROID_TOOLCHAIN_DIR/bin:$PATH
export AR=arm-linux-androideabi-ar
export CC=arm-linux-androideabi-gcc
export CXX=arm-linux-androideabi-g++
export LINK=arm-linux-androideabi-g++
export PLATFORM=android

gyp --depth=. -Dtarget_arch=arm -DOS=android -Dpomelo_library=static_library -Duse_sys_openssl=false -Dbuild_jpomelo=true $SSL pomelo.gyp --format=make

