#!/bin/bash

export ANDROID_TOOLCHAIN_DIR=$PWD/android-toolchain
export PATH=$ANDROID_TOOLCHAIN_DIR/bin:$PATH
export AR=arm-linux-androideabi-ar
export CC=arm-linux-androideabi-gcc
export CXX=arm-linux-androideabi-g++
export LINK=arm-linux-androideabi-g++
export PLATFORM=android

gyp --depth=. -Dtarget_arch=arm -DOS=android -Dpomelo_library=static_library -Duse_sys_openssl=false -Dbuild_jpomelo=true pomelo.gyp --format=make
