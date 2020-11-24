doxygen = find_program('doxygen', required : false)
if get_option('enable-docs') and doxygen.found()
    conf_data = configuration_data()
    conf_data.set('DOXYGEN_INPUT', public_header_dir)
    conf_data.set('DOXYGEN_OUTPUT', meson.current_build_dir())
    doxyfile = configure_file(
        input: 'Doxyfile.in',
        output: 'Doxyfile',
        configuration: conf_data
    )
    custom_target('docs',
      input: doxyfile,
      build_by_default: true,
      command: [doxygen, '@INPUT@'],
      output: ['html']
    )
endif
