project('phosphor-host-ipmid', 'cpp', version: '0.1')

# Expose headers as <phosphor-ipmi-host/...> without building any targets.
# Creates a symlink: $builddir/phosphor-ipmi-host -> $srcdir (headers live at repo root)
run_command(
  'ln', '-sfT',
  meson.current_source_dir(),
  meson.current_build_dir() / 'phosphor-ipmi-host',
  check: true)

phosphor_host_ipmid_dep = declare_dependency(
  include_directories: include_directories('.', 'include'))
