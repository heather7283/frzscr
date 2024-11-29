project(
    'frzscr', 'c',
    version: '0.1.0',
    license: 'GPL-3.0-or-later',
    default_options: ['buildtype=release', 'warning_level=3']
)

add_project_arguments('-Wno-gnu-zero-variadic-macro-arguments', language: 'c')
add_project_arguments('-Wno-unused-parameter', language: 'c')
add_project_arguments('-Wno-unused-variable', language: 'c')

libm = meson.get_compiler('c').find_library('m')

wayland_scanner = find_program('wayland-scanner')
wayland_client_dep = dependency('wayland-client')

protocols = ['wlr-layer-shell-unstable-v1', 'wlr-screencopy-unstable-v1', 'xdg-shell']
protocol_sources = []

foreach protocol : protocols
    in_file = '@0@.xml'.format(join_paths('protocols', protocol))
    out_h = '@0@-client.h'.format(protocol)
    out_c = '@0@-protocol.c'.format(protocol)

    header = custom_target(out_h,
        output: out_h,
        input: in_file,
        command: [ wayland_scanner, 'client-header', '@INPUT@', '@OUTPUT@' ],
    )

    source = custom_target(out_c,
        output: out_c,
        input: in_file,
        command: [ wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@' ],
    )

    protocol_sources += [header]
    protocol_sources += [source]
endforeach

executable('frzscr',
    'frzscr.c',
    'wayland.c',
    'window.c',
    'shm.c',
    'screenshot.c',
    protocol_sources,
    dependencies: [ wayland_client_dep, libm ],
    install: true
)
