check_dep = dependency('check')

ls_inc = include_directories('..')

ls_objects = main.extract_objects(lease_server_files)
ls_test_sources = [
   'lease-server-test.c',
   'test-socket-client.c',
]

ls_test = executable('lease-server-test',
           sources: ls_test_sources,
           objects: ls_objects,
           dependencies: [check_dep, fff_dep, dlmcommon_dep],
           include_directories: ls_inc)

test('DRM Lease manager - socket server test', ls_test, is_parallel: false)
