project('zlib', 'c', version : '1.2.11', license : 'zlib')

link_args = []

cc = meson.get_compiler('c')

if cc.get_id() == 'gcc' and host_machine.system() == 'linux'
  vflag = '-Wl,--version-script,@0@/zlib.map'.format(meson.current_source_dir())
  link_args += [vflag]
endif

conf = configuration_data()
conf.set('Z_HAVE_UNISTD_H', cc.check_header('unistd.h'))

# FIXME: we could avoid this if we moved the sources down into a src subdir...
rename_zconf = find_program('meson/rename-zconf.py')
status = run_command(rename_zconf, meson.current_source_dir() / 'zconf.h', check: true)
if status.stdout() != ''
  message(status.stdout())
endif

configure_file(
  input: 'zconf.h.cmakein',
  output: 'zconf.h',
  format: 'cmake',
  configuration: conf,
  install: true,
  install_dir: 'include',
)

pkg = import('pkgconfig')

src = [
  'adler32.c',
  'compress.c',
  'crc32.c',
  'deflate.c',
  'gzclose.c',
  'gzlib.c',
  'gzread.c',
  'gzwrite.c',
  'infback.c',
  'inffast.c',
  'inflate.c',
  'inftrees.c',
  'trees.c',
  'uncompr.c',
  'zutil.c',
]

headers = ['zlib.h']

zlib = library('z', src,
  c_args : ['-DZLIB_DLL'],
  link_args : link_args,
  install : true,
  version : meson.project_version())

incdir = include_directories('.')

zlib_dep = declare_dependency(link_with : zlib, include_directories : incdir)

install_headers(headers)

pkg.generate(name: 'zlib',
             description: 'zlib compression library',
             version: meson.project_version(),
             libraries: zlib)

if get_option('tests').enabled()
  example = executable('example', 'test/example.c', link_with: zlib)
  test('example', example)
endif
