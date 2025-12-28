project('dead_editor', ['c', 'cpp'],
  version : '0.5.0',
  default_options : ['cpp_std=c++23', 'c_std=c11', 'warning_level=2']
)

generate_version_script = files('scripts/generate_version.sh')
version_h = custom_target('version_h',
  input : 'Version.h.in',
  output : 'Version.h',
  command : [
    'sh', generate_version_script,
    '@INPUT@', '@OUTPUT@', meson.project_version(), meson.project_source_root()
  ],
  build_by_default : true
)

sdl2_dep = dependency('sdl2')
sdl2_ttf_dep = dependency('SDL2_ttf')
ts_core_dep = dependency('tree-sitter')

if host_machine.system() == 'linux'
    util_dep = meson.get_compiler('cpp').find_library('util')
else
    util_dep = dependency('', required: false)
endif

libvterm_proj = subproject('libvterm')
vterm_dep = libvterm_proj.get_variable('vterm_dep')

ts_cpp_proj = subproject('tree-sitter-cpp')
ts_cpp_dep = ts_cpp_proj.get_variable('tree_sitter_cpp_dep')

ts_c_proj = subproject('tree-sitter-c')
ts_c_dep = ts_c_proj.get_variable('tree_sitter_c_dep')

ts_python_proj = subproject('tree-sitter-python')
ts_python_dep = ts_python_proj.get_variable('tree_sitter_python_dep')

ts_markdown_proj = subproject('tree-sitter-markdown')
ts_markdown_dep = ts_markdown_proj.get_variable('tree_sitter_markdown_dep')

ts_yaml_proj = subproject('tree-sitter-yaml')
ts_yaml_dep = ts_yaml_proj.get_variable('tree_sitter_yaml_dep')

ts_lua_proj = subproject('tree-sitter-lua')
ts_lua_dep = ts_lua_proj.get_variable('tree_sitter_lua_dep')

ts_zig_proj = subproject('tree-sitter-zig')
ts_zig_dep = ts_zig_proj.get_variable('tree_sitter_zig_dep')

ts_diff_proj = subproject('tree-sitter-diff')
ts_diff_dep = ts_diff_proj.get_variable('tree_sitter_diff_dep')

ts_meson_proj = subproject('tree-sitter-meson')
ts_meson_dep = ts_meson_proj.get_variable('tree_sitter_meson_dep')

ts_toml_proj = subproject('tree-sitter-toml')
ts_toml_dep = ts_toml_proj.get_variable('tree_sitter_toml_dep')

ts_json_proj = subproject('tree-sitter-json')
ts_json_dep = ts_json_proj.get_variable('tree_sitter_json_dep')

ts_javascript_proj = subproject('tree-sitter-javascript')
ts_javascript_dep = ts_javascript_proj.get_variable('tree_sitter_javascript_dep')

ts_html_proj = subproject('tree-sitter-html')
ts_html_dep = ts_html_proj.get_variable('tree_sitter_html_dep')

ts_css_proj = subproject('tree-sitter-css')
ts_css_dep = ts_css_proj.get_variable('tree_sitter_css_dep')

sources = [
  'src/main.cpp',
  'src/Application.cpp',
  'src/Utils.cpp',
  'src/Syntax.cpp',
  'src/TextureCache.cpp',
  'src/FileTree.cpp',
  'src/Terminal.cpp',
  'src/TextDocument.cpp',
  'src/EditorView.cpp',
  'src/EditorController.cpp',
  'src/Editor.cpp',
  'src/LanguageRegistry.cpp',
  'src/CommandManager.cpp',
]

dead_editor = executable('DeadEditor',
  sources,
  version_h,
  dependencies : [
    sdl2_dep, sdl2_ttf_dep, ts_core_dep, util_dep, vterm_dep,
    ts_cpp_dep, ts_c_dep, ts_python_dep, ts_markdown_dep,
    ts_yaml_dep, ts_lua_dep, ts_zig_dep, ts_diff_dep, ts_meson_dep,
    ts_toml_dep, ts_json_dep, ts_javascript_dep, ts_html_dep, ts_css_dep
  ],
  install : true
)

configure_file(
  input : 'JetBrainsMonoNLNerdFont-Regular.ttf',
  output : 'JetBrainsMonoNLNerdFont-Regular.ttf',
  copy : true
)

configure_file(
  input : 'icon.png',
  output : 'icon.png',
  copy : true
)

ffmpeg = find_program('ffmpeg', required : false)
if ffmpeg.found()
  icon_bmp = custom_target('icon_bmp',
    input : 'icon.png',
    output : 'icon.bmp',
    command : [ffmpeg, '-y', '-i', '@INPUT@', '-vf', 'scale=256:256', '@OUTPUT@'],
    build_by_default : true
  )
endif

if host_machine.system() == 'darwin'
  bash = find_program('bash')

  generate_icns_script = files('scripts/generate_icns.sh')
  icon_icns = custom_target('icon_icns',
    input : 'icon.png',
    output : 'DeadEditor.icns',
    command : [bash, generate_icns_script, '@INPUT@', '@OUTPUT@'],
    build_by_default : true
  )

  create_bundle_script = files('scripts/create_app_bundle.sh')
  app_bundle = custom_target('app_bundle',
    input : [dead_editor, icon_icns],
    output : 'DeadEditor.app',
    command : [bash, create_bundle_script, meson.current_build_dir(), meson.current_source_dir(), meson.project_version()],
    depends : [dead_editor, icon_icns],
    build_by_default : true
  )

  create_dmg_script = files('scripts/create_dmg.sh')
  dmg_filename = 'DeadEditor-' + meson.project_version() + '-macos.dmg'
  dmg = custom_target('dmg',
    input : [app_bundle],
    output : dmg_filename,
    command : [bash, create_dmg_script, meson.current_build_dir(), meson.project_version()],
    depends : [app_bundle, icon_icns],
    build_by_default : false
  )
endif

if host_machine.system() == 'linux'
  install_data('packaging/linux/DeadEditor.desktop',
    install_dir : get_option('datadir') / 'applications'
  )

  install_data('JetBrainsMonoNLNerdFont-Regular.ttf',
    install_dir : get_option('datadir') / 'DeadEditor'
  )

  if ffmpeg.found()
    bash = find_program('bash')
    generate_icons_script = files('scripts/generate_linux_icons.sh')

    linux_icons = custom_target('linux_icons',
      input : 'icon.png',
      output : 'icons',
      command : [bash, generate_icons_script, '@INPUT@', '@OUTPUT@'],
      build_by_default : true
    )
  endif

  icon_sizes = ['16', '24', '32', '48', '64', '128', '256', '512']
  foreach size : icon_sizes
    install_data('icon.png',
      install_dir : get_option('datadir') / 'icons' / 'hicolor' / size + 'x' + size / 'apps',
      rename : 'deadedit.png'
    )
  endforeach
endif
