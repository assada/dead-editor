project('tree-sitter-zig', 'c')

src = files('src/parser.c')
inc = include_directories('src')

lib = static_library('tree-sitter-zig',
    src,
    include_directories : inc,
    c_args : ['-w']
)

tree_sitter_zig_dep = declare_dependency(
    link_with : lib,
    include_directories : inc
)
