project('tree-sitter-toml', 'c')

src = files('src/parser.c', 'src/scanner.c')
inc = include_directories('src')

lib = static_library('tree-sitter-toml',
    src,
    include_directories : inc,
    c_args : ['-w']
)

tree_sitter_toml_dep = declare_dependency(
    link_with : lib,
    include_directories : inc
)
