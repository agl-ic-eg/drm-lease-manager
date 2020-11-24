libdlmcommon_sources = [
        'socket-path.c',
        'log.c'
]

libdlmcommon = static_library(
        'common',
        sources: libdlmcommon_sources,
        include_directories : configuration_inc,
)

dlmcommon_dep = declare_dependency(
    link_with : libdlmcommon,
    include_directories : include_directories('.')
)
