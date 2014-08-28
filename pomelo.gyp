{
  'target_defaults': {
    'conditions': [
      ['OS == "win"', {
        'msvs_settings': {
          'VCCLCompilerTool': {
            'AdditionalOptions': [ '/TP' ],
          }
        },
        'defines': [
          '_WIN32',
          'WIN32',
          '_CRT_NONSTDC_NO_DEPRECATE',
          '_DEBUG',
          '_WINDOWS',
          '_USRDLL',
          'JANSSON_DLL_EXPORTS',
          '_WINDLL',
          '_UNICODE',
          'UNICODE'
        ],
        'link_settings': {
          'libraries': [
            '-ladvapi32.lib',
            '-liphlpapi.lib',
            '-lpsapi.lib',
            '-lshell32.lib',
            '-lws2_32.lib'
          ],
        },
      }],   # OS == "win"
      ['OS != "win" ',{
        'defines':[
          '_LARGEFILE_SOURCE',
          '_FILE_OFFSET_BITS=64',
          '_GNU_SOURCE',
        ]
      }],   # OS != "win"
      ['TO == "ios"', {
        'xcode_settings': {
          'TARGETED_DEVICE_FAMILY': '1,2',
          'CODE_SIGN_IDENTITY': 'iPhone Developer',
          'IPHONEOS_DEPLOYMENT_TARGET': '5.0',
          'ARCHS': '$(ARCHS_STANDARD_32_64_BIT)',
        },
      }], # TO == "ios"
    ],    # conditions
  },

  'targets': [
    {
      'target_name': 'libpomelo',
      'type': '<(library)',
      'include_dirs': [
        './include',
        './src',
        './deps/jansson/src',
        './deps/uv/include',
      ],
      'dependencies': [
        'deps/uv/uv.gyp:libuv',
        'deps/jansson/jansson.gyp:jansson',
      ],
      'link_settings': {
          'libraries': [
            '-lssl',
            '-lcrypto',
          ],
      },
     
      'sources': [
        './src/pc_pomelo.c',
        './src/pc_lib.c',
        './src/pc_trans.c',
        './src/pc_trans_repo.c',
        './src/tr/dummy/tr_dummy.c',
        './src/tr/uv/pr_pkg.c',
        './src/tr/uv/tr_uv_tcp.c',
        './src/tr/uv/tr_uv_tcp_i.c',
        './src/tr/uv/tr_uv_tcp_aux.c',
        './src/tr/uv/tr_uv_tls.c',
        './src/tr/uv/tr_uv_tls_i.c',
        './src/tr/uv/tr_uv_tls_aux.c',
        './src/tr/uv/pr_msg.c',
        './src/tr/uv/pr_msg_pb.c',
        './src/tr/uv/pr_msg_json.c',
        './src/tr/uv/pb_util.c',
        './src/tr/uv/pb_decode.c',
        './src/tr/uv/pb_encode.c',
     ],
      'conditions': [
        ['OS != "win"', {
          'ldflags': [
            '-no-undefined',
            '-export-symbols-regex \'^json_\'',
            '-version-info 8:0:4',
          ],
          'cflags': [
            '-ggdb',
          ]
        }]    # OS != "win"
      ],    # conditions
    },
    {
       'target_name': 'tr_dummy',
       'type': 'executable',
       'dependencies': [
          'libpomelo',
       ],
       'include_dirs': [
          './include/',
       ],
          'cflags': [
            '-ggdb',
          ],
        'sources': [
           './test/test-tr_dummy.c'
       ],
    },

    {
       'target_name': 'tr_tls',
       'type': 'executable',
       'dependencies': [
          'libpomelo',
       ],
       'include_dirs': [
          './include/',
       ],
          'cflags': [
            '-ggdb',
          ],
        'sources': [
           './test/test-tr_tls.c'
       ],
    },
    {
       'target_name': 'tr_tcp',
       'type': 'executable',
       'dependencies': [
          'libpomelo',
       ],
       'include_dirs': [
          './include/',
          './deps/uv/include',
       ],
          'cflags': [
            '-ggdb',
          ],
        'sources': [
           './test/test-tr_tcp.c'
       ],
    },

  ]
}
