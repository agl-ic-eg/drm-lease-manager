project(
    'lease_manager', 'c',
    # Version based on Semantic Versioning 2.0.0 (https://semver.org/)
    version: '0.1.0',
    default_options: [
        'warning_level=2',
    ]
)

config = configuration_data()

pkg = import('pkgconfig')

compiler_flags = [
    '-Wstrict-prototypes',
    '-Wmissing-prototypes',
]

config.set_quoted('STATEDIR', get_option('localstatedir'))

cc = meson.get_compiler('c')
add_project_arguments(
    cc.get_supported_arguments(compiler_flags),
    language: 'c'
)

configure_file(output: 'config.h',
               configuration: config)

configuration_inc = include_directories('.')

drm_dep = dependency('libdrm', version: '>= 2.4.89')

enable_tests = get_option('enable-tests')

if enable_tests
  check_dep = dependency('check')

# Default to the system provided version of fff.h.
# otherwise fetch it ourselves.
  if cc.check_header('fff.h')
    fff_dep = declare_dependency()
  else
    if meson.version().version_compare('>=0.55')
      fff_proj = subproject('fff')
      fff_dep = fff_proj.get_variable('fff_dep')
    else
      error('Update meson version to >=0.55 to enable unit testing')
    endif
  endif
endif

subdir('common')
subdir('drm-lease-manager')
subdir('libdlmclient')
subdir('examples')
