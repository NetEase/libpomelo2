libpomelo2
===============

### How to compile 

#### gyp

    $ gyp --depth=. pomelo.gyp [options]

options:


- -Dno_tls_support=[true | false], `false` by default

disable tls support

- -Duse_sys_openssl=[true | false], `true` by default

enable openssl, but use system pre-install libssl & libcrypto, if `false`, it will compile openssl from source code in deps/openssl.

- -Dno_uv_support=[true | false], `false` by default

disable uv support, it also disable tls support as tls implementation is based on uv.

- -Duse_sys_uv=[true | false], `false` by default

use system pre-install libuv, similar to `use_sys_openssl`, if enable, the pre-install libuv version should be 0.11.x

- -Duse_sys_jansson=[true | false], `false` by default

use system pre-install jansson.

- -Dpomelo_library=[static_library | shared_library], `static_library` by default

static library or shared library for libpomelo2

- -Dtarget_arch=[ia32 | x64 ], `ia32` by default.

if you enable openssl and do not use system pre-install openssl, this option is used when compiling openssl from deps/openssl

- -Dbuild_pypomelo=[true | false], `false` by default.
- -Dpython_header=<include path>, `/usr/include/python2.7` by default.

These two options is used to configure compilation for pypomelo.

- -Dbuild_jpomelo=[true|false], `false` by default.

configure jpomelo compilation for java
