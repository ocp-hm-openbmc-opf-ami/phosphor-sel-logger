project(
  'phosphor-sel-logger',
  'cpp',
  version: '0.1',
  meson_version: '>=1.1.1',
  default_options: [
    'werror=true',
    'warning_level=3',
    'cpp_std=c++23',
  ])

cpp_args = []

if get_option('tests').enabled()
  cpp_args += ['-Wno-error=return-type', '-Wno-error=unused-function',
               '-Wno-error=unused-variable']
  sdbusplus_dep = subproject('sdbusplus').get_variable('sdbusplus_dep')
  meson.override_dependency('sdbusplus',sdbusplus_dep)
  phosphor_dbus_interfaces_dep = dependency('phosphor-dbus-interfaces-ami')
else
  sdbusplus_dep = dependency('sdbusplus')
endif
deps = [
  sdbusplus_dep,
  dependency('libsystemd'),
  dependency('boost'),
]

if get_option('log-threshold')
  cpp_args += '-DSEL_LOGGER_MONITOR_THRESHOLD_EVENTS'
endif
if get_option('log-pulse')
  cpp_args += '-DREDFISH_LOG_MONITOR_PULSE_EVENTS'
endif
if get_option('log-watchdog')
  cpp_args += '-DSEL_LOGGER_MONITOR_WATCHDOG_EVENTS'
endif
if get_option('log-alarm')
  cpp_args += '-DSEL_LOGGER_MONITOR_THRESHOLD_ALARM_EVENTS'
endif
if get_option('log-host')
  cpp_args += '-DSEL_LOGGER_MONITOR_HOST_ERROR_EVENTS'
endif
if get_option('send-to-logger')
    if get_option('tests').enabled()
      # send-to-logger produces src conflicts    
      deps += subproject('phosphor-logging').get_variable('phosphor_logging_dep')
    else
      cpp_args += '-DSEL_LOGGER_SEND_TO_LOGGING_SERVICE'
      deps += dependency('phosphor-logging')
    endif
endif

if get_option('log-crash')
   cpp_args += '-DSEL_LOGGER_LOG_CRASH'
 endif

if get_option('sel-delete')
  cpp_args += '-DSEL_LOGGER_ENABLE_SEL_DELETE'
endif

if get_option('sel-extended')
  cpp_args += '-DSEL_LOGGER_ENABLE_SEL_EXTENDED'
endif

if get_option('static-sensor-number')
  cpp_args += '-DFEATURE_STATIC_SENSOR_NUMBER'
endif

if get_option('tests').enabled()
  deps += subproject('intel-ipmi-oem').get_variable('intel_ipmi_oem_dep')
  deps += subproject('phosphor-host-ipmid').get_variable('phosphor_host_ipmid_dep')
endif

executable(
  'sel-logger',
  'src/sel_logger.cpp',
  include_directories: include_directories('include'),
  implicit_include_directories: false,
  cpp_args: cpp_args,
  dependencies: deps,
  install: true,
  install_dir: get_option('bindir'))

systemd = dependency('systemd')
if systemd.found()
  install_data(
    'service_files/xyz.openbmc_project.Logging.IPMI.service',
    install_dir: systemd.get_variable('systemdsystemunitdir'))
endif

if get_option('tests').enabled()
  subdir('tests')
endif
