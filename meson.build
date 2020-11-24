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

subdir('common')
subdir('drm-lease-manager')
subdir('libdlmclient')
subdir('examples')
