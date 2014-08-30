{
  'variables': {
    'uv_library%': "static_library",
    'pomelo_library%': "static_library",
    'target_arch%': "ia32",
    'use_sys_openssl%': "true",
    'library%': "static_library",
    'use_sys_uv%': "false",
    'use_sys_jansson%': "false",
    'no_tls_support%': "false",
    'no_uv_support%': "false",
    'build_pypomelo%': "false",
    'python_header%': "/usr/include/python2.7",
  },

  'target_defaults': {
    'conditions': [
      ['OS == "win"', {
        'msvs_settings': {
          'VCCLCompilerTool': {
            'AdditionalOptions': [ '/TP' ],
          }
        },
        'link_settings': {
          'libraries': [
            '-ladvapi32.lib',
            '-liphlpapi.lib',
            '-lpsapi.lib',
            '-lshell32.lib',
            '-lws2_32.lib'
          ],
        },
      },   
      {  # else 
        'defines':[
          '_LARGEFILE_SOURCE',
          '_FILE_OFFSET_BITS=64',
          '_GNU_SOURCE',
        ]
      }],   # OS == "win"
    ],    # conditions

    'conditions': [
      ['use_sys_jansson == "false"', {
        'dependencies': [
          './deps/jansson/jansson.gyp:jansson',
        ],

        'include_dirs': [
          './deps/jansson/src',
        ]
      }, {
        'link_settings': {
          'libraries': [
            '-ljansson',
            ]
        }
      }], # use_sys_jansson

    [ 'no_uv_support == "false"', {
      'conditions' : [
        ['use_sys_uv == "false"', {
          'dependencies': [
            './deps/uv/uv.gyp:libuv',
            ],

            'include_dirs': [
              './deps/uv/include',
            ]
        }, {
          'link_settings': {
            'libraries': [
              '-luv',
            ]
          }
        }], # use_sys_uv
        
        [ 'no_tls_support == "false"', {
          'conditions': [
            ['use_sys_openssl == "false"', {
              'dependencies': [
                './deps/openssl/openssl.gyp:openssl',
                ],

                'include_dirs': [
                  './deps/openssl/openssl/include',
                ]
            }, {
              'link_settings': {
                'libraries': [
                  '-lssl',
                  '-lcrypto',
                  ]
              }
            }], # use_sys_openssl
          ],
        }],  # no tls support
      ]
    }], # no uv support
    ],
  },

  'targets': [
  {
    'target_name': 'libpomelo2',
    'type': '<(pomelo_library)',
    'include_dirs': [
      './include',
      './src',
    ],

    'sources': [
      './src/pc_pomelo.c',
      './src/pc_lib.c',
      './src/pc_trans.c',
      './src/pc_trans_repo.c',
      './src/tr/dummy/tr_dummy.c',
    ],
    'conditions': [
      [ 'OS != "win"', {
        'cflags': [
          '-ggdb',
        ],
      }],
      ['no_uv_support == "false"', {
        'sources': [
          './src/tr/uv/pr_msg.c',
          './src/tr/uv/pr_msg_pb.c',
          './src/tr/uv/pr_msg_json.c',
          './src/tr/uv/pb_util.c',
          './src/tr/uv/pb_decode.c',
          './src/tr/uv/pb_encode.c',
          './src/tr/uv/pr_pkg.c',
          './src/tr/uv/tr_uv_tcp.c',
          './src/tr/uv/tr_uv_tcp_i.c',
          './src/tr/uv/tr_uv_tcp_aux.c',
        ],
        'conditions': [
          ['no_tls_support == "false"', {
            'sources': [
              './src/tr/uv/tr_uv_tls.c',
              './src/tr/uv/tr_uv_tls_i.c',
              './src/tr/uv/tr_uv_tls_aux.c',
          ]}, {
            'defines': [
              'PC_NO_UV_TLS_TRANS',
            ]}
          ], # no tls support
        ]}, {
          'defines': [
            'PC_NO_UV_TCP_TRANS',
        ]}
      ], # no uv support
    ],
  },

    {
     'target_name': 'tr_dummy',
     'type': 'executable',
     'dependencies': [
        'libpomelo2',
     ],
     'include_dirs': [
        './include/',
     ],
     'cflags': [
        '-ggdb',
     ],
     'sources': [
        './test/test-tr_dummy.c',
     ],
  },

  {
     'target_name': 'tr_tls',
     'type': 'executable',
     'dependencies': [
        'libpomelo2',
     ],
     'include_dirs': [
        './include/',
     ],
     'cflags': [
       '-ggdb',
     ],
     'sources': [
         './test/test-tr_tls.c',
     ],
  },
  {
    'target_name': 'tr_tcp',
    'type': 'executable',
    'dependencies': [
      'libpomelo2',
    ],
    'include_dirs': [
      './include/',
    ],
    'cflags': [
      '-ggdb',
    ],
    'sources': [
      './test/test-tr_tcp.c',
    ],
  },

  ],
  'conditions': [
    [
      'build_pypomelo == "true"', {
       'targets':[ {
    'target_name': 'pypomelo',
    'type': 'shared_library',
    'dependencies': [
      'libpomelo2',
    ],
    'include_dirs': [
      './include/',
      '<(python_header)',
    ],
    'cflags': [
      '-ggdb',
    ],
    'sources': [
      './py/pypomelo.c',
    ],
  },],

   }]
  ],
}
