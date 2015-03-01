{
  'variables': {
    'platform%': 'pc'
   },
  'conditions': [
    ['platform == "ios"', {
      'xcode_settings': {
        'SDKROOT': 'iphoneos',
      }, # xcode_settings
    }],  # platform == "ios"
  ],  # conditions

  'targets': [
    {
      'target_name': 'jansson',
      'type': 'static_library',
      'include_dirs': [
        './src'
      ],

      'sources': [
        'src/dump.c',
        'src/error.c',
        'src/hashtable.c',
        'src/hashtable_seed.c',
        'src/load.c',
        'src/memory.c',
        'src/pack_unpack.c',
        'src/strbuffer.c',
        'src/strconv.c',
        'src/utf.c',
        'src/value.c',
      ],
      'defines': [
          'HAVE_STDINT_H',
          'VALGRIND',
      ],
      'conditions': [
        ['OS == "win"', {
          'defines': [
            'WIN32',
            '_CRT_NONSTDC_NO_DEPRECATE',
            '_WINDOWS',
            '_UNICODE',
            'UNICODE'
          ]
        }],   # OS == "win"
        ['OS != "win"',{
          'ldflags': [
            '-no-undefined',
            '-export-symbols-regex \'^json_\'',
            '-version-info 8:0:4',
          ],
          'cflags': [
            '-fPIC',
          ],
        }],   # OS != "win"
      ],    # conditions
    },
  ],
}
