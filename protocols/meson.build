protocols = [
    'wlr-layer-shell-unstable-v1',
    'wlr-screencopy-unstable-v1',
    'xdg-shell',
    'xdg-output-unstable-v1',
    'fractional-scale-v1',
    'viewporter'
]
protocol_sources = []

foreach protocol: protocols
    in_file = '@0@.xml'.format(protocol)
    out_h = '@0@-client.h'.format(protocol)
    out_c = '@0@-protocol.c'.format(protocol)

    header = custom_target(out_h,
        output: out_h,
        input: in_file,
        command: [wayland_scanner, 'client-header', '@INPUT@', '@OUTPUT@'],
    )

    source = custom_target(out_c,
        output: out_c,
        input: in_file,
        command: [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@'],
    )

    protocol_sources += [header]
    protocol_sources += [source]
endforeach

