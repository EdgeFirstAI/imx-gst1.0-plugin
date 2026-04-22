# The i.MX ASRC Plugin
gstimxasrc_sources = [
  'gstimxasrc.c',
  'imxasrc-converter.c',
  'imxasrc-resampler.c',
  'imxasrc-lib-hw.c',
  'imxasrc-lib-sw.c',
  'imxasrc-utils.c',
  'imxasrc-device.c'
]
gstimxasrc_headers = [
  'gstimxasrc.h',
  'imxasrc-converter.h',
  'imxasrc-resampler.h',
  'imxasrc-lib-hw.h',
  'imxasrc-lib-sw.h',
  'imxasrc-utils.h',
  'imxasrc-device.h'
]

gstimxasrcexample = library('gstimxasrc',
  gstimxasrc_sources,
  c_args: version_flags,
  link_args : gst_plugin_ldflags + ['-lasound'] + ['-ldl'],
  include_directories : [extinc, libsinc, pluginsinc],
  dependencies : [gst_dep, gst_base_dep, gst_plugins_base_dep, gsttag_dep, gstriff_dep, gstfsl_dep, gst_audio_dep],
  install : true,
  install_dir : plugins_install_dir,
)
