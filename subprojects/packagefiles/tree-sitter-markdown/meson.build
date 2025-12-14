project('tree-sitter-markdown', 'c')

block_src = files(
    'tree-sitter-markdown/src/parser.c',
    'tree-sitter-markdown/src/scanner.c'
)
block_inc = include_directories('tree-sitter-markdown/src')

inline_src = files(
    'tree-sitter-markdown-inline/src/parser.c',
    'tree-sitter-markdown-inline/src/scanner.c'
)
inline_inc = include_directories('tree-sitter-markdown-inline/src')

block_lib = static_library('tree-sitter-markdown-block',
    block_src,
    include_directories : block_inc,
    c_args : ['-w']
)

inline_lib = static_library('tree-sitter-markdown-inline',
    inline_src,
    include_directories : inline_inc,
    c_args : ['-w']
)

tree_sitter_markdown_dep = declare_dependency(
    link_with : [block_lib, inline_lib],
    include_directories : [block_inc, inline_inc]
)
