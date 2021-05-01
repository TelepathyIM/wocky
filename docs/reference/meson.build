
gnome.gtkdoc(meson.project_name(),
  main_sgml: 'wocky-docs.sgml',
  dependencies: wocky_dep,
  src_dir: [
    meson.current_build_dir()/'..'/'..'/'wocky',
    meson.current_source_dir()/'..'/'..'/'wocky',
  ],
  namespace: 'wocky',
  mkdb_args: [ '--sgml-mode', '--output-format=xml'],
  scan_args: ['--rebuild-types', '--rebuild-sections'],
  install: true
)
