project('tree-sitter-php', 'c')

src = files('php/src/parser.c', 'php/src/scanner.c')
inc = include_directories('php/src')

lib = static_library('tree-sitter-php',
    src,
    include_directories : inc,
    c_args : ['-w']
)

tree_sitter_php_dep = declare_dependency(
    link_with : lib,
    include_directories : inc
)

