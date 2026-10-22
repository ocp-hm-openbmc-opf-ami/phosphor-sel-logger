project('intel-ipmi-oem', 'cpp', version: '0.1')

# Expose headers as <intel-ipmi-oem/...> without building any targets.
# Creates a symlink: $builddir/intel-ipmi-oem -> $srcdir/include
run_command(
  'ln', '-sfT',
  meson.current_source_dir() / 'include',
  meson.current_build_dir() / 'intel-ipmi-oem',
  check: true)

intel_ipmi_oem_dep = declare_dependency(
  include_directories: include_directories('.'))
