libpomelo2
===============

### How to compile 

#### Install [gyp](https://gyp.gsrc.io/)
```
git clone https://chromium.googlesource.com/external/gyp
cd gyp
python setup.py install

```
#### Generate native IDE project files by [gyp](https://gyp.gsrc.io/)

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

- -Dbuild_pypomelo=[true | false], `false` by default.
- -Dpython_header=<include path>, `/usr/include/python2.7` by default.

These two options is used to configure compilation for pypomelo.

- -Dbuild_jpomelo=[true|false], `false` by default.

configure jpomelo compilation for java

- -Dbuild_cspomelo=[true|false], `false` by default.

configure cspomelo compilation for c#
