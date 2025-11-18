recorderengine_sources = [ 'recorder_engine.c']
recorderengine_headers = [ 'recorder_engine.h', 'fsl_types.h']

gstpbutils_dep = cc.find_library('gstpbutils-' + api_version)

install_headers(recorderengine_headers, subdir : 'gstreamer-1.0/gst/imx-mm/recorder_engine/')
gstapp_dep = dependency('gstreamer-app-1.0', version : gst_req,
    fallback : ['gst-plugins-base', 'app_dep'])

recorderengine = library('recorder_engine-@0@'.format(api_version),
  recorderengine_sources + recorderengine_headers,
  version : '0.0.0',
  soversion : 0,
  install : true,
  dependencies : [gst_dep, gst_base_dep, gst_plugins_base_dep, gst_video_dep, gstpbutils_dep, gstapp_dep],
)

recorderengine_dep = declare_dependency(link_with : recorderengine)

src_file = ['grecorder.c']

executable('grecorder-' + api_version,
  src_file,
  install: true,
  link_args : ['-pthread'],
  dependencies : [gst_dep, recorderengine_dep],
)