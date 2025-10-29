fs = import('fs')

wl_protocols_dep = dependency('wayland-protocols', version: '>=1.41', default_options: ['tests=false'])
wl_protocols_dir = wl_protocols_dep.get_variable(pkgconfig: 'pkgdatadir')

protocols = [
  'wlr-layer-shell-unstable-v1',
  'wlr-screencopy-unstable-v1',
  wl_protocols_dir / 'stable' / 'xdg-shell' / 'xdg-shell',
  wl_protocols_dir / 'stable' / 'viewporter' / 'viewporter',
  wl_protocols_dir / 'staging' / 'ext-image-capture-source' / 'ext-image-capture-source-v1',
  wl_protocols_dir / 'staging' / 'ext-foreign-toplevel-list' / 'ext-foreign-toplevel-list-v1',
  wl_protocols_dir / 'staging' / 'ext-image-copy-capture' / 'ext-image-copy-capture-v1',
  wl_protocols_dir / 'unstable' / 'xdg-output' / 'xdg-output-unstable-v1',
]
protocol_sources = []

foreach protocol : protocols
  base_name = fs.name(protocol)
  in_file = '@0@.xml'.format(protocol)
  out_h = '@0@.h'.format(base_name)
  out_c = '@0@.c'.format(base_name)

  header = custom_target(
    out_h,
    output: out_h,
    input: in_file,
    command: [wayland_scanner, 'client-header', '@INPUT@', '@OUTPUT@'],
  )

  source = custom_target(
    out_c,
    output: out_c,
    input: in_file,
    command: [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@'],
  )

  protocol_sources += [header]
  protocol_sources += [source]
endforeach
