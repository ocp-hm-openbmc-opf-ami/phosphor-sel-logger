# Test build config for phosphor-sel-logger.
# All targets here are test-only; production build is not affected.

# Prefer system-installed gtest/gmock; fall back to the bundled subproject.
gtest_dep = dependency('gtest', main: false, required: false)
gmock_dep = dependency('gmock', required: false)

if not gtest_dep.found() or not gmock_dep.found()
  cmake = import('cmake')
  opt_var = cmake.subproject_options()
  opt_var.add_cmake_defines({'CMAKE_CXX_FLAGS': '-Wno-pedantic'})
  opt_var.set_override_option('warning_level', '3')
  opt_var.set_override_option('werror', 'true')

  gtest_proj = cmake.subproject('gtest', options: opt_var, required: false)
  if gtest_proj.found()
    gtest_dep = declare_dependency(
      dependencies: [dependency('threads'), gtest_proj.dependency('gtest')])
    gmock_dep = gtest_proj.dependency('gmock')
  else
    # Fall back to native Meson subproject layout.
    gtest_proj = subproject('gtest')
    gtest_dep = gtest_proj.get_variable('gtest_dep')
    gmock_dep = gtest_proj.get_variable('gmock_dep')
  endif
endif

# Carry over production cpp_args and Boost defines.
test_cpp_args = [
  '-Wno-error=return-type',
  '-Wno-error=unused-function',
  '-Wno-error=unused-variable',
  '-DBOOST_ALL_NO_LIB',
  '-DBOOST_SYSTEM_NO_DEPRECATED',
  '-DBOOST_ERROR_CODE_HEADER_ONLY',
]

# sdbusplus is already overridden to the subproject build in root meson.build.
sdbusplus_dep = dependency('sdbusplus')

# phosphor-logging is required even when send-to-logger=false because
# sel_logger.hpp unconditionally includes <phosphor-logging/log.hpp>.
phosphor_logging_dep = subproject('phosphor-logging').get_variable(
  'phosphor_logging_dep')

# intel-ipmi-oem and phosphor-host-ipmid provide the inline sensor helpers
# (getSensorNumberFromPath, getSensorTypeFromPath, etc.) used by sel_logger.hpp.
intel_ipmi_oem_dep = subproject('intel-ipmi-oem').get_variable(
  'intel_ipmi_oem_dep')
phosphor_host_ipmid_dep = subproject('phosphor-host-ipmid').get_variable(
  'phosphor_host_ipmid_dep')

# ── test_sensorutils ────────────────────────────────────────────────────────
# Tests for pure-math ipmi:: functions in include/sensorutils.hpp.
# No production source file needed (header-only, stdlib only).
test_sensorutils_exe = executable(
  'test_sensorutils',
  [
    'test_main.cpp',
    'test_sensorutils.cpp',
  ],
  include_directories: include_directories('..'),
  dependencies: [gtest_dep, gmock_dep],
  cpp_args: test_cpp_args,
  link_args: ['-lstdc++fs'],
)
test('unit_test_sensorutils', test_sensorutils_exe)

# ── test_sel_logger_utils ────────────────────────────────────────────────────
# Tests for toHexStr() and isValidSensorType() from include/sel_logger.hpp.
# -Wl,--allow-multiple-definition is not needed here because we do not link
# src/sel_logger.cpp (which contains main()).
test_sel_logger_utils_exe = executable(
  'test_sel_logger_utils',
  [
    'test_main.cpp',
    'test_sel_logger_utils.cpp',
  ],
  include_directories: include_directories('../include'),
  dependencies: [
    gtest_dep,
    gmock_dep,
    sdbusplus_dep,
    phosphor_logging_dep,
    intel_ipmi_oem_dep,
    phosphor_host_ipmid_dep,
    dependency('boost'),
    dependency('libsystemd'),
  ],
  cpp_args: test_cpp_args,
  link_args: ['-lstdc++fs'],
)
test('unit_test_sel_logger_utils', test_sel_logger_utils_exe)
