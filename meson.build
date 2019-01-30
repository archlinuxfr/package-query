project(
  'package-query', 'c',
  version: '1.9',
  license: 'GPL-2.0-or-later',
  default_options: [
    'warning_level=2',
  ],
)

compiler = meson.get_compiler('c')

add_project_arguments(
  '-D_GNU_SOURCE',
  language: 'c'
)

# Checks for libraries
alpm_dep = dependency ('libalpm', version: '>= 11.0.0')
yajl_dep = dependency ('yajl')
curl_dep = dependency ('libcurl', version: '>= 7.19.4')

# Checks for header files
check_headers = [
  'ctype.h',
  'getopt.h',
  'glob.h',
  'libintl.h',
  'limits.h',
  'locale.h',
  'regex.h',
  'signal.h',
  'sys/ioctl.h',
  'sys/stat.h',
  'sys/utsname.h',
]

foreach h: check_headers
  if not compiler.has_header(h)
    error('Header file @0@ not found.'.format(h))
  endif
endforeach

# Define build configuration
package_query_conf_data = configuration_data()

aur_base_url = get_option('aur-url')
root_dir = get_option('root-dir')
pacman_conf = join_paths(get_option('prefix'), get_option('sysconfdir'), 'pacman.conf')
pacman_db_path = join_paths(get_option('prefix'), get_option('localstatedir'), 'lib', 'pacman')

package_query_conf_data.set_quoted('PACKAGE_NAME', meson.project_name())
package_query_conf_data.set_quoted('PACKAGE_VERSION', meson.project_version())
package_query_conf_data.set_quoted('AUR_BASE_URL', aur_base_url)
package_query_conf_data.set_quoted('CONFFILE', pacman_conf)
package_query_conf_data.set_quoted('DBPATH', pacman_db_path)
package_query_conf_data.set_quoted('LOCALEDIR', join_paths(get_option('prefix'), get_option('localedir')))
package_query_conf_data.set_quoted('ROOTDIR', root_dir)

# Compute git version
git = find_program('git', required: false)

if git.found()
  git_describe = run_command(
    git.path(),
    '-C', meson.current_source_dir(),
    'describe', '--abbrev=4',
  )
endif

if git.found() and git_describe.returncode() == 0
  use_git_version = true
  package_query_git_version = git_describe.stdout().strip()
else
  use_git_version = false
  package_query_git_version = ''
endif

package_query_conf_data.set('USE_GIT_VERSION', use_git_version)
package_query_conf_data.set_quoted('GIT_VERSION', package_query_git_version)

# Generate config.h
configure_file(
  configuration: package_query_conf_data,
  output: 'config.h',
)

# package-query executable
package_query_sources = [
  'src/alpm-query.c',
  'src/alpm-query.h',
  'src/aur.c',
  'src/aur.h',
  'src/color.c',
  'src/color.h',
  'src/package-query.c',
  'src/util.c',
  'src/util.h',
]

package_query_dependencies = [
  alpm_dep,
  yajl_dep,
  curl_dep,
]

executable(
  'package-query',
  sources: package_query_sources,
  dependencies: package_query_dependencies,
  install: true,
)

# Man page
install_man('doc/package-query.8')

# Configuration summary
message(
  '\n@0@:\n'.format(meson.project_name())
  + '\n'
  + '  Build information:\n'
  + '    source code location   : @0@\n'.format(meson.current_source_dir())
  + '    prefix                 : @0@\n'.format(get_option('prefix'))
  + '    sysconfdir             : @0@\n'.format(join_paths(get_option('prefix'), get_option('sysconfdir')))
  + '       conf file           : @0@\n'.format(pacman_conf)
  + '    localstatedir          : @0@\n'.format(join_paths(get_option('prefix'), get_option('localstatedir')))
  + '       database dir        : @0@\n'.format(pacman_db_path)
  + '    compiler               : @0@\n'.format(compiler.get_id())
  + '    compilation type       : @0@\n'.format(get_option('buildtype'))
  + '\n'
  + '    package-query version  : @0@\n'.format(meson.project_version())
  + '    using git version      : @0@\n'.format(use_git_version)
  + '       git ver             : @0@\n'.format(package_query_git_version)
  + '\n'
  + '  Variable information:\n'
  + '    root working directory : @0@\n'.format(root_dir)
  + '    aur base url           : @0@\n'.format(aur_base_url)
)
