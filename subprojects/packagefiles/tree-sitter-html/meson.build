project('tree-sitter-html', 'c')

src = files('src/parser.c', 'src/scanner.c')
inc = include_directories('src')

lib = static_library('tree-sitter-html',
    src,
    include_directories : inc,
    c_args : ['-w']
)

tree_sitter_html_dep = declare_dependency(
    link_with : lib,
    include_directories : inc
)
