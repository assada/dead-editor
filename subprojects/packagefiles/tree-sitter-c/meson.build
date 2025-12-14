project('tree-sitter-c', 'c')

src = files('src/parser.c')
inc = include_directories('src')

lib = static_library('tree-sitter-c',
    src,
    include_directories : inc,
    c_args : ['-w']
)

tree_sitter_c_dep = declare_dependency(
    link_with : lib,
    include_directories : inc
)
