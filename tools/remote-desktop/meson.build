src_files = ['remote_input.c', 'main.c']
header_files = [ 'remote_input.h']

libinput_dep = dependency('libinput', version: '>= 1.2.0')
libudev_dep = dependency('libudev', version: '>= 136')
glib_dep = dependency('glib-2.0', version: '>= 2.64.0',
  fallback: ['glib', 'libglib_dep'])

if cc.has_header('linux/uinput.h')
  executable('remote_input',
    src_files + header_files,
    install: true,
    dependencies : [libinput_dep, libudev_dep, glib_dep],
  )
endif
install_data(sources: 'remote_desktop.py', install_dir : '/usr/bin/')