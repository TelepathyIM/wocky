
TEST_DIR = meson.current_source_dir()
CERT_DIR = TEST_DIR / 'certs'
CA_DIR = CERT_DIR / 'cas'
CRL_DIR = CERT_DIR / 'crl'

keys = {
  'CA_KEY': 'ca-0-key.pem',
  'SS_KEY': 'ss-key.pem',
  'REV_KEY': 'rev-key.pem',
  'EXP_KEY': 'exp-key.pem',
  'NEW_KEY': 'new-key.pem',
  'INS_KEY': 'ins-key.pem',
  'WILD_KEY': 'wild-key.pem',
  'SERVER_KEY': 'tls-key.pem',
  'BADWILD_KEY': 'badwild-key.pem',
  'UNKNOWN_KEY': 'unknown-key.pem'
}
certs = {
  'CA_CRT': 'ca-0-cert.pem',
  'SS_CRT': 'ss-cert.pem',
  'REV_CRT': 'rev-cert.pem',
  'EXP_CRT': 'exp-cert.pem',
  'NEW_CRT': 'new-cert.pem',
  'INS_CRT': 'ins-cert.pem',
  'WILD_CRT': 'wild-cert.pem',
  'SERVER_CRT': 'tls-cert.pem',
  'BADWILD_CRT': 'badwild-cert.pem',
  'UNKNOWN_CRT': 'unknown-cert.pem'
}

tests = {
  'wocky-bare-contact-test': [
    'wocky-test-helper.c', 'wocky-test-helper.h',
    'wocky-test-stream.c', 'wocky-test-stream.h',
    'wocky-bare-contact-test.c',
  ],
  'wocky-caps-hash-test': [
    'wocky-test-helper.c', 'wocky-test-helper.h',
    'wocky-test-stream.c', 'wocky-test-stream.h',
    'wocky-caps-hash-test.c',
  ],
  'wocky-connector-test': [
    'wocky-test-sasl-auth-server.c',
    'wocky-test-sasl-auth-server.h',
    'wocky-test-connector-server.c',
    'wocky-test-connector-server.h',
    'wocky-test-helper.c', 'wocky-test-helper.h',
    'wocky-test-stream.c', 'wocky-test-stream.h',
    'test-resolver.c', 'test-resolver.h',
    'wocky-connector-test.c',
  ],
  'wocky-contact-factory-test': [
    'wocky-test-helper.c', 'wocky-test-helper.h',
    'wocky-test-stream.c', 'wocky-test-stream.h',
    'wocky-contact-factory-test.c',
  ],
  'wocky-data-form-test': [
    'wocky-test-helper.c', 'wocky-test-helper.h',
    'wocky-test-stream.c', 'wocky-test-stream.h',
    'wocky-data-form-test.c',
  ],
  'wocky-jid-validation-test': [
    'wocky-test-helper.c', 'wocky-test-helper.h',
    'wocky-test-stream.c', 'wocky-test-stream.h',
    'wocky-jid-validation-test.c',
  ],
  'wocky-loopback-test': [
    'wocky-test-helper.c', 'wocky-test-helper.h',
    'wocky-test-stream.c', 'wocky-test-stream.h',
    'wocky-loopback-test.c',
  ],
  'wocky-node-tree-test': [
    'wocky-test-helper.c', 'wocky-test-helper.h',
    'wocky-test-stream.c', 'wocky-test-stream.h',
    'wocky-node-tree-test.c',
  ],
  'wocky-pep-service-test': [
    'wocky-test-helper.c', 'wocky-test-helper.h',
    'wocky-test-stream.c', 'wocky-test-stream.h',
    'wocky-pep-service-test.c',
  ],
  'wocky-ping-test': [
    'wocky-test-helper.c', 'wocky-test-helper.h',
    'wocky-test-stream.c', 'wocky-test-stream.h',
    'wocky-ping-test.c',
  ],
  'wocky-porter-test': [
    'wocky-test-helper.c', 'wocky-test-helper.h',
    'wocky-test-stream.c', 'wocky-test-stream.h',
    'wocky-porter-test.c',
  ],
  'wocky-pubsub-node-test': [
    'wocky-test-helper.c', 'wocky-test-helper.h',
    'wocky-test-stream.c', 'wocky-test-stream.h',
    'wocky-pubsub-test-helpers.c', 'wocky-pubsub-test-helpers.h',
    'wocky-pubsub-node-test.c',
  ],
  'wocky-pubsub-service-test': [
    'wocky-test-helper.c', 'wocky-test-helper.h',
    'wocky-test-stream.c', 'wocky-test-stream.h',
    'wocky-pubsub-test-helpers.c', 'wocky-pubsub-test-helpers.h',
    'wocky-pubsub-service-test.c',
  ],
  'wocky-resource-contact-test': [
    'wocky-test-helper.c', 'wocky-test-helper.h',
    'wocky-test-stream.c', 'wocky-test-stream.h',
    'wocky-resource-contact-test.c',
  ],
  'wocky-roster-test': [
    'wocky-test-helper.c', 'wocky-test-helper.h',
    'wocky-test-stream.c', 'wocky-test-stream.h',
    'wocky-roster-test.c',
  ],
  'wocky-sasl-utils-test': [
    'wocky-sasl-utils-test.c',
  ],
  'wocky-scram-sha1-test': [
    'wocky-test-helper.c', 'wocky-test-helper.h',
    'wocky-test-stream.c', 'wocky-test-stream.h',
    'wocky-scram-sha1-test.c',
  ],
  'wocky-test-sasl-auth': [
    'wocky-test-helper.c', 'wocky-test-helper.h',
    'wocky-test-stream.c', 'wocky-test-stream.h',
    'wocky-test-sasl-handler.c',
    'wocky-test-sasl-handler.h',
    'wocky-test-sasl-auth-server.c',
    'wocky-test-sasl-auth-server.h',
    'wocky-test-sasl-auth.c',
  ],
  'wocky-session-test': [
    'wocky-test-helper.c', 'wocky-test-helper.h',
    'wocky-test-stream.c', 'wocky-test-stream.h',
    'wocky-session-test.c',
  ],
  'wocky-stanza-test': [
    'wocky-test-helper.c', 'wocky-test-helper.h',
    'wocky-test-stream.c', 'wocky-test-stream.h',
    'wocky-stanza-test.c',
  ],
  'wocky-tls-test': [
    'wocky-test-helper.c', 'wocky-test-helper.h',
    'wocky-test-stream.c', 'wocky-test-stream.h',
    'wocky-tls-test.c',
  ],
  'wocky-sm-test': [
    'wocky-test-sasl-auth-server.c',
    'wocky-test-sasl-auth-server.h',
    'wocky-test-connector-server.c',
    'wocky-test-connector-server.h',
    'wocky-test-helper.c', 'wocky-test-helper.h',
    'wocky-test-stream.c', 'wocky-test-stream.h',
    'test-resolver.c', 'test-resolver.h',
    'wocky-sm-test.c',
  ],
  'wocky-utils-test': [
    'wocky-utils-test.c',
  ],
  'wocky-xmpp-connection-test': [
    'wocky-test-helper.c', 'wocky-test-helper.h',
    'wocky-test-stream.c', 'wocky-test-stream.h',
    'wocky-xmpp-connection-test.c',
  ],
  'wocky-xmpp-node-test': [
    'wocky-test-helper.c', 'wocky-test-helper.h',
    'wocky-test-stream.c', 'wocky-test-stream.h',
    'wocky-xmpp-node-test.c',
  ],
  'wocky-xmpp-reader-test': [
    'wocky-test-helper.c', 'wocky-test-helper.h',
    'wocky-test-stream.c', 'wocky-test-stream.h',
    'wocky-xmpp-reader-test.c',
  ],
  'wocky-xmpp-readwrite-test': [
    'wocky-test-stream.c', 'wocky-test-stream.h',
    'wocky-test-helper.c', 'wocky-test-helper.h',
    'wocky-xmpp-readwrite-test.c',
  ],
}

dummy_server_src = [
  'wocky-dummy-xmpp-server.c',
  'wocky-test-connector-server.c', 'wocky-test-connector-server.h',
  'wocky-test-helper.c', 'wocky-test-helper.h',
  'wocky-test-stream.c', 'wocky-test-stream.h',
  'wocky-test-sasl-auth-server.c', 'wocky-test-sasl-auth-server.h',
]

test_deps = wocky_deps + [ wocky_dep ]

if libsasl_dep.found()
  test_deps += libsasl_dep
endif

tls_cflags = [
  '-DTLS_CA_DIR="@0@"'.format(CA_DIR),
  '-DTLS_CRL_DIR="@0@"'.format(CRL_DIR)
]

foreach key, file: keys
  path = CERT_DIR / file
  tls_cflags += '-DTLS_@0@_FILE="@1@"'.format(key, path)
endforeach

foreach cert, file: certs
  path = CERT_DIR / file
  tls_cflags += '-DTLS_@0@_FILE="@1@"'.format(cert, path)
endforeach

test_cflags = ['-DG_LOG_DOMAIN="Wocky-Test"' ]

tls_progs = ['wocky-connector-test','wocky-sm-test', 'wocky-tls-test']
sasl_progs = ['wocky-connector-test', 'wocky-sm-test', 'wocky-test-sasl-auth']

executable('wocky-dummy-xmpp-server', files(dummy_server_src),
  dependencies: test_deps,
  include_directories: [ wocky_conf_inc ],
  c_args: [ test_cflags, tls_cflags],
  link_with: wocky_so)

check_src = []
foreach prog, src: tests
  prog_cflags = test_cflags
  prog_deps = test_deps
  if get_option('code-style-check')
    foreach file: src
      if file not in check_src
        check_src += file
      endif
    endforeach
  endif
  if prog in tls_progs
    prog_cflags += tls_cflags
  endif
  exe = executable(prog, files(src),
    dependencies: prog_deps,
    include_directories: [ wocky_conf_inc ],
    c_args: prog_cflags,
    link_with: wocky_so)
  test(prog, exe,
    is_parallel: prog not in sasl_progs,
    env: [
      'WOCKY_CHANNEL_BINDING_TYPE=tls-exporter'
    ])
  if prog == 'wocky-connector-test'
    test(prog+'-tls1.2', exe,
      is_parallel: false,
      env: [
        'G_TLS_GNUTLS_PRIORITY=NORMAL:%COMPAT:-VERS-TLS1.3',
        'G_TLS_OPENSSL_MAX_PROTO=0x0303'
      ])
  endif
endforeach

if get_option('code-style-check')
  foreach file: dummy_server_src
    if file not in check_src
      check_src += file
    endif
  endforeach
  wocky_check_files += files(check_src)
endif
