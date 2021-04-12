project(
  'wocky','c',
  license: 'LGPL2.1+',
  version: '0.19.0',
  meson_version: '>= 0.49.0',
  default_options: ['c_std=c99', 'warning_level=2', 'werror=true']
)

prefix = get_option('prefix')
libdir = get_option('libdir')
includedir = get_option('includedir')

if get_option('install-headers') != ''
  includedir = includedir/get_option('install-headers')
endif

if get_option('libdir-suffix') != ''
  libdir = libdir / get_option('libdir-suffix')
endif

# compiler flags
common_flags = [
  '-D_GNU_SOURCE',
  '-DHAVE_CONFIG_H',
  '-DG_LOG_USE_STRUCTURED',
]
add_project_arguments(common_flags, language: 'c')

# compiler specific flags
werr_cflags = []
warn_errs = [
  #  'all',
  #  'extra',
  'declaration-after-statement',
  'implicit-function-declaration',
  'shadow',
  'strict-prototypes',
  'missing-prototypes',
  'sign-compare',
  'nested-externs',
  'pointer-arith',
  'format',
  'format-security',
  'init-self'
]
no_warn_errs = [
  'missing-field-initializers',
  'cast-function-type',
  'unused-parameter'
]
foreach e: warn_errs
  werr_cflags += '-W@0@'.format(e)
endforeach

## covered by werror=true
#werr_cflags += '-Werror'

foreach e: no_warn_errs
  werr_cflags += '-Wno-@0@'.format(e)
  werr_cflags += '-Wno-error=@0@'.format(e)
endforeach

cc = meson.get_compiler('c')
add_project_arguments(cc.get_supported_arguments(werr_cflags), language: 'c')

# dependencies
glib_dep    = dependency('glib-2.0',    version: '>= 2.52',
  fallback: ['glib', 'libglib_dep'])
gio_dep     = dependency('gio-2.0',     version: '>= 2.52',
  fallback: ['glib', 'libgio_dep'])
gobj_dep    = dependency('gobject-2.0',
  fallback: ['glib', 'gobj_dep'])
gmod_dep    = dependency('gmodule-2.0',
  fallback: ['glib', 'gmod_dep'])
libxml2_dep = dependency('libxml-2.0')
sqlite3_dep = dependency('sqlite3')
libsasl_dep = dependency('libsasl2', required: false)
libiphb_dep = dependency('libiphb', version: '>= 0.61.31', required: false)
libsoup_dep = dependency('libsoup-2.4', version: '>= 2.42', required: get_option('google-relay'))

certtool = find_program('certtool', required: false)

# configuration
conf = configuration_data()
conf_h = ''
defines = []

check_h = [
  'dlfcn.h',
  'inttypes.h',
  'memory.h',
  'stdint.h',
  'stdlib.h',
  'unistd.h',
  'string.h',
  'strings.h',
  'sys/stat.h',
  'sys/types.h'
]

foreach h: check_h
  if cc.has_header(h)
    define = 'HAVE_' + h.underscorify().to_upper()
    defines += define
  endif
endforeach

if get_option('debug')
  defines += 'ENABLE_DEBUG'
endif

if get_option('google-relay')
  defines += 'ENABLE_GOOGLE_RELAY'
endif

if libiphb_dep.found()
  defines += 'HAVE_LIBIPHB'
endif

if libsasl_dep.found()
  defines += 'HAVE_LIBSASL2'
endif

foreach define: defines
  conf.set(define, 1)
  conf_h += '#define @0@ 1\n'.format(define)
endforeach

config_h = configure_file(
  output: 'config.h',
  configuration: conf
)

gnome = import('gnome')
pymod = import('python')
python = pymod.find_installation()

shell = find_program('sh')

# Introspection
gir = find_program('g-ir-scanner', required : get_option('introspection').enabled())
build_gir = gir.found() and not meson.is_cross_build() and get_option('introspection').enabled()

output = '\nConfigure summary\n\n'
output += '  Prefix:              ' + prefix + '\n'
output += '  Libdir:              ' + libdir + '\n'
output += '  Install includes:    ' + includedir + '\n'
#output += '  Coverage profiling:  ' + gcov_dep.found().to_string() + '\n'
output += '  Coding style checks: ' + get_option('code-style-check').to_string() + '\n'
output += '  Debug:               ' + get_option('debug').to_string() + '\n\n'
output += 'Features:\n'
output += '  Has GnuTLS:          ' + certtool.found().to_string() + '\n'
output += '  SASL2 Tests:         ' + libsasl_dep.found().to_string() + '\n'
output += '  libiphb integration: ' + libiphb_dep.found().to_string() + '\n'
output += '  google jingle relay: ' + get_option('google-relay').to_string() + '\n'
output += '  gtk-doc generation:  ' + get_option('gtk_doc').to_string() +'\n'
output += '  g-ir generation:     ' + build_gir.to_string() +'\n'
message(output)

wocky_inc = include_directories('wocky')
wocky_dep_inc = '-I'+meson.current_source_dir()
wocky_conf_inc = include_directories('.')

wocky_check_files = []
subdir('wocky')
subdir('tests')

if get_option('code-style-check')
  run_target('check', command: [
    shell, meson.source_root()/'tools'/'check-c-style.sh', wocky_check_files
  ])
endif

if get_option('gtk_doc')
  subdir('docs/reference')
endif
