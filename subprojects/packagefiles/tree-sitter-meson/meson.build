project('tree-sitter-meson', 'c')

src = files('src/parser.c')
inc = include_directories('src')

lib = static_library('tree-sitter-meson',
    src,
    include_directories : inc,
    c_args : ['-w']
)

tree_sitter_meson_dep = declare_dependency(
    link_with : lib,
    include_directories : inc
)
