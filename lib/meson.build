lib_inc = include_directories('.')

libobjtracker = static_library('objtracker',
    sources: [
      'obj_tracker.c',
      'oracle.c',
    ],
    include_directories: [root_inc],
    dependencies: [
      libpthread_dep, 
      libdl_dep,
    ],
    c_args: c_args,
    install: false,
)

libobjtracker_dep = declare_dependency(
    link_with: libobjtracker,
    include_directories: [root_inc, lib_inc],
)

shared_library('objtracker',
    sources: [
      'blas_tracker.c',
      'obj_tracker.c',
    ],
    include_directories: [root_inc],
    dependencies: [
      libpthread_dep, 
      libdl_dep,
    ],
    c_args: c_args + ['-DSTANDALONE', '-DTRACE_OUTPUT'],
    link_args: '-Wl,-init,obj_tracker_init,-fini,obj_tracker_fini,-eentry',
    install: true,
)
