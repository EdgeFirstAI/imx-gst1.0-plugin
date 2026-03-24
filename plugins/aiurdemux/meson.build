aiurdemux_cflags = []
if platform_is_aarch64 == 'no'
  aiurdemux_cflags += ['-D_ARM11']
endif

aiurdemux_sources = [ 'aiur.c', 'aiurregistry.c', 'aiurstreamcache.c', 'aiuridxtab.c', 'aiurdemux.c', 'aiurtypefind.c', 'aiurcontent.c']
gstaiurdemux = library('gstaiurdemux',
  aiurdemux_sources,
  c_args: version_flags + aiurdemux_cflags,
  link_args : gst_plugin_ldflags + ['-ldl'],
  include_directories : [extinc, libsinc],
  dependencies : [gst_dep, gst_base_dep, gst_plugins_base_dep, gsttag_dep, gstriff_dep, gstfsl_dep, imx_parser_dep],
  install : true,
  install_dir : plugins_install_dir,
)

imxparser_libdir = join_paths(imx_parser_dep.get_pkgconfig_variable('libdir'), 'imx-mm/parser')

aiur_registry = configuration_data()
aiur_registry_install_dir = '@0@/share'.format(get_option('prefix'))

# asf/rm binary release core parser still need arm11/arm9 version
check_imxparser = [
  ['asf', 'arm9'],
  ['asf', 'arm11'],
  ['asf', 'arm'],
  ['avi', 'arm'],
  ['flv', 'arm'],
  ['mpg2', 'arm'],
  ['mp4', 'arm'],
  ['mkv', 'arm'],
  ['ogg', 'arm'],
  ['rm', 'arm9'],
  ['rm', 'arm11'],
  ['rm', 'arm'],
  ['ape', 'arm'],
]

foreach parser : check_imxparser
  filename = '@0@/@1@/lib_@2@_parser_@3@_elinux.so'.format(sys_root, imxparser_libdir, parser.get(0),parser.get(1))
  soname = run_command(python3, '../find_soname.py', filename).stdout().strip()
  if soname == 'file_not_exist'
    soname = 'lib_@0@_parser_@1@_elinux.so'.format(parser.get(0),parser.get(1))
  endif
  aiur_registry.set('@0@_@1@_SONAME'.format(parser.get(0),parser.get(1)), '@0@/@1@'.format(imxparser_libdir,soname))
endforeach

aiur_registry_files = [
  'aiur_registry_1.0',
]

if platform_is_aarch64 == 'yes'
  foreach p : aiur_registry_files
    infile = p + '.arm.cf.in'
    outfile = p + '.arm.cf'
    configure_file(input : infile,
      output : outfile,
      configuration : aiur_registry,
      install_dir : aiur_registry_install_dir)
  endforeach
else
  foreach p : aiur_registry_files
    infile = p + '.arm11.cf.in'
    outfile = p + '.arm11.cf'
    configure_file(input : infile,
      output : outfile,
      configuration : aiur_registry,
      install_dir : aiur_registry_install_dir)
  endforeach
endif