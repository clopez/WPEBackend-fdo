project('WPEBackend-fdo', 'cpp',
	default_options: ['cpp_std=c++11'],
	version: '0.1',
)

hdr = [
	'include/wpe-fdo/initialize-egl.h',
	'include/wpe-fdo/view-backend-exportable.h',
]
src = [
	'src/fdo.cpp',
	'src/initialize-egl.cpp',
	'src/input-libxkbcommon.cpp',
	'src/renderer-backend-egl.cpp',
	'src/renderer-host.cpp',
	'src/view-backend-exportable-fdo.cpp',
	'src/ws.cpp',
]

cxx = meson.get_compiler('cpp')
if not cxx.has_header('xkbcommon/xkbcommon-keysyms.h')
	error('The <xkbcommon/xkbcommon-keysyms.h> header is missing')
endif

add_project_arguments(
	cxx.get_supported_arguments(['-fno-rtti', '-fno-exceptions']),
	language: 'cpp'
)

dep = [
	dependency('egl'),
	dependency('gio-2.0'),
	dependency('wayland-client'),
	dependency('wayland-server'),
	dependency('wayland-egl'),
	dependency('wpe'),
]

lib = shared_library(meson.project_name(), src,
	include_directories: include_directories('include'),
	version: meson.project_version(),
	dependencies: dep,
	install: true,
)

install_headers(hdr, subdir: 'wpe-fdo')

import('pkgconfig').generate(
	name: 'wpebackend-fdo',
	description: 'The WPEBackend-fdo library',
	libraries: lib,
	version: meson.project_version(),
)
