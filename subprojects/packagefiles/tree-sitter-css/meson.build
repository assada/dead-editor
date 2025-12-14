project('tree-sitter-css', 'c')

src = files('src/parser.c', 'src/scanner.c')
inc = include_directories('src')

lib = static_library('tree-sitter-css',
    src,
    include_directories : inc,
    c_args : ['-w']
)

tree_sitter_css_dep = declare_dependency(
    link_with : lib,
    include_directories : inc
)
