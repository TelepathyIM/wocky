
handwritten_headers = files(
  'wocky.h',
  'wocky-auth-handler.h',
  'wocky-auth-registry.h',
  'wocky-bare-contact.h',
  'wocky-c2s-porter.h',
  'wocky-caps-cache.h',
  'wocky-ll-connection-factory.h',
  'wocky-caps-hash.h',
  'wocky-connector.h',
  'wocky-contact.h',
  'wocky-contact-factory.h',
  'wocky-data-form.h',
  'wocky-debug.h',
  'wocky-disco-identity.h',
  'wocky-google-relay.h',
  'wocky-jabber-auth.h',
  'wocky-jabber-auth-digest.h',
  'wocky-jabber-auth-password.h',
  'wocky-jingle-content.h',
  'wocky-jingle-factory.h',
  'wocky-jingle-info-internal.h',
  'wocky-jingle-info.h',
  'wocky-jingle-media-rtp.h',
  'wocky-jingle-session.h',
  'wocky-jingle-transport-google.h',
  'wocky-jingle-transport-iceudp.h',
  'wocky-jingle-transport-iface.h',
  'wocky-jingle-transport-rawudp.h',
  'wocky-jingle-types.h',
  'wocky-ll-connector.h',
  'wocky-ll-contact.h',
  'wocky-loopback-stream.h',
  'wocky-meta-porter.h',
  'wocky-muc.h',
  'wocky-namespaces.h',
  'wocky-node.h',
  'wocky-node-tree.h',
  'wocky-pep-service.h',
  'wocky-ping.h',
  'wocky-porter.h',
  'wocky-pubsub-helpers.h',
  'wocky-pubsub-node.h',
  'wocky-pubsub-node-protected.h',
  'wocky-pubsub-service.h',
  'wocky-pubsub-service-protected.h',
  'wocky-resource-contact.h',
  'wocky-roster.h',
  'wocky-sasl-auth.h',
  'wocky-sasl-utils.h',
  'wocky-sasl-digest-md5.h',
  'wocky-sasl-scram.h',
  'wocky-sasl-plain.h',
  'wocky-session.h',
  'wocky-stanza.h',
  'wocky-tls.h',
  'wocky-tls-handler.h',
  'wocky-tls-connector.h',
  'wocky-types.h',
  'wocky-utils.h',
  'wocky-xep-0115-capabilities.h',
  'wocky-xmpp-connection.h',
  'wocky-xmpp-error.h',
  'wocky-xmpp-reader.h',
  'wocky-xmpp-writer.h'
)

handwritten_sources = files(
  'wocky.c',
  'wocky-auth-handler.c',
  'wocky-auth-registry.c',
  'wocky-bare-contact.c',
  'wocky-c2s-porter.c',
  'wocky-caps-cache.c',
  'wocky-ll-connection-factory.c',
  'wocky-caps-hash.c',
  'wocky-connector.c',
  'wocky-contact.c',
  'wocky-contact-factory.c',
  'wocky-data-form.c',
  'wocky-debug.c',
  'wocky-debug-internal.h',
  'wocky-disco-identity.c',
  'wocky-heartbeat-source.c',
  'wocky-heartbeat-source.h',
  'wocky-google-relay.c',
  'wocky-jabber-auth.c',
  'wocky-jabber-auth-digest.c',
  'wocky-jabber-auth-password.c',
  'wocky-jingle-content.c',
  'wocky-jingle-factory.c',
  'wocky-jingle-info.c',
  'wocky-jingle-media-rtp.c',
  'wocky-jingle-session.c',
  'wocky-jingle-transport-google.c',
  'wocky-jingle-transport-iceudp.c',
  'wocky-jingle-transport-iface.c',
  'wocky-jingle-transport-rawudp.c',
  'wocky-ll-connector.c',
  'wocky-ll-contact.c',
  'wocky-loopback-stream.c',
  'wocky-meta-porter.c',
  'wocky-muc.c',
  'wocky-node.c',
  'wocky-node-private.h',
  'wocky-node-tree.c',
  'wocky-pep-service.c',
  'wocky-ping.c',
  'wocky-porter.c',
  'wocky-pubsub-helpers.c',
  'wocky-pubsub-node.c',
  'wocky-pubsub-node-internal.h',
  'wocky-pubsub-service.c',
  'wocky-resource-contact.c',
  'wocky-roster.c',
  'wocky-sasl-auth.c',
  'wocky-sasl-digest-md5.c',
  'wocky-sasl-scram.c',
  'wocky-sasl-utils.c',
  'wocky-sasl-plain.c',
  'wocky-session.c',
  'wocky-stanza.c',
  'wocky-utils.c',
  'wocky-tls.c',
  'wocky-tls-common.c',
  'wocky-tls-handler.c',
  'wocky-tls-connector.c',
  'wocky-xep-0115-capabilities.c',
  'wocky-xmpp-connection.c',
  'wocky-xmpp-error.c',
  'wocky-xmpp-reader.c',
  'wocky-xmpp-writer.c'
)

if get_option('code-style-check')
  wocky_check_files += handwritten_sources
  wocky_check_files += handwritten_headers
endif

enumtype_sources = [
  'wocky-auth-registry.h',
  'wocky-connector.h',
  'wocky-data-form.h',
  'wocky-jingle-info-internal.h',
  'wocky-jingle-types.h',
  'wocky-muc.h',
  'wocky-pubsub-node.h',
  'wocky-pubsub-service.h',
  'wocky-tls.h',
  'wocky-xmpp-error.h',
  'wocky-xmpp-reader.h'
  ]

wocky_marshal_list = 'wocky-signals-marshal.list'
wocky_marshal_src = custom_target('wocky-signals-marshal',
  input: handwritten_sources,
  output: wocky_marshal_list,
  command: [python, meson.current_source_dir()/'..'/'tools'/'gen_marshal_list.py', '@INPUT@', '@OUTPUT@' ]
)
wocky_marshals = gnome.genmarshal('wocky-signals-marshal',
  sources: [wocky_marshal_src],
  install_header: (includedir != ''),
  install_dir: includedir/meson.project_name(),
  prefix: '_@0@_signals_marshal'.format(meson.project_name()))

wocky_enums = gnome.mkenums_simple('wocky-enumtypes',
  body_prefix: '#include "config.h"',
  install_header: (includedir != ''),
  install_dir: includedir/meson.project_name(),
  sources: enumtype_sources
)

wocky_sources = [
  handwritten_sources,
  handwritten_headers,
  wocky_marshals,
  wocky_enums,
]

wocky_deps = [glib_dep, gio_dep, gobj_dep, gmod_dep, sqlite3_dep, libxml2_dep ]

if libiphb_dep.found()
  wocky_deps += libiphb_dep
endif

if libsoup_dep.found()
  wocky_deps += libsoup_dep
endif

wocky_cflags = ['-DG_LOG_DOMAIN="wocky"', '-DWOCKY_COMPILATION']
wocky_ldflags = []

wocky_ldflags += cc.get_supported_link_arguments(['-Wl,--no-undefined'])

install_headers(handwritten_headers,
  install_dir: includedir/meson.project_name())

wocky_so = library(
  meson.project_name(), wocky_sources,
  install: true,
  install_dir: libdir,
  version: meson.project_version(),
  dependencies: wocky_deps,
  include_directories: [wocky_inc, wocky_conf_inc],
  c_args: wocky_cflags,
  link_args: wocky_ldflags
)

pkg = import('pkgconfig')
pkg.generate(wocky_so,
  filebase: meson.project_name(),
  subdirs: get_option('install-headers'),
)


gir = find_program('g-ir-scanner', required: get_option('introspection').enabled())
if gir.found()
  wocky_gir = gnome.generate_gir(wocky_so,
    sources: [handwritten_headers, handwritten_sources, wocky_marshals, wocky_enums],
    dependencies: wocky_deps,
    namespace: 'Wocky',
    nsversion: '0',
    symbol_prefix: ['wocky'],
    header: 'wocky.h',
    install: build_gir,
    extra_args: wocky_cflags
  )
endif

wocky_dep = declare_dependency(
  dependencies: wocky_deps,
  sources: [ wocky_enums[1], wocky_marshals[1] ],
  compile_args: wocky_dep_inc,
  include_directories: wocky_inc,
  link_with: wocky_so,
  link_args: wocky_ldflags)
