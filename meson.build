project('librender', 'c')

# Native libs
cc = meson.get_compiler('c')
dl = cc.find_library('dl')

# Subproject dependencies
error_proj       = subproject('error')
sized_types_proj = subproject('sized_types')
libwindow_proj   = subproject('libwindow')

error       = error_proj.get_variable('error_dep')
sized_types = sized_types_proj.get_variable('sized_types_dep')
libwindow   = libwindow_proj.get_variable('libwindow_dep')

librender = library(
  'render',
  'src/render.c',
  dependencies: [
    error,
    sized_types,
    libwindow,
    dl
  ]
)

librender_dirs = include_directories('src')
librender_dep = declare_dependency(
  link_with: librender,
  include_directories: librender_dirs,
  dependencies: [
    error,
    sized_types,
    libwindow
  ]
)
