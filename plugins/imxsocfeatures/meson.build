gstimxsocfeaturesplugin = library('gstimxsocfeaturesplugin',
  'gstimxsocfeaturesplugin.c',
  c_args: version_flags,
  dependencies: [gst_dep, gstfsl_dep],
  include_directories : [libsinc, pluginsinc],
  install: true,
  install_dir: plugins_install_dir,
)