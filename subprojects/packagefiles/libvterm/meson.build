project('libvterm', 'c',
  version : '0.3',
  license : 'MIT',
  default_options : ['c_std=c99', 'warning_level=0']
)

inc = include_directories('include')

sources = files(
  'src/encoding.c',
  'src/keyboard.c',
  'src/mouse.c',
  'src/parser.c',
  'src/pen.c',
  'src/screen.c',
  'src/state.c',
  'src/unicode.c',
  'src/vterm.c'
)

libvterm = static_library('vterm',
  sources,
  include_directories : inc,
  install : false,
  c_args : ['-D_XOPEN_SOURCE=600', '-D_POSIX_C_SOURCE=200809L']
)

vterm_dep = declare_dependency(
  include_directories : inc,
  link_with : libvterm
)
