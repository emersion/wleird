project(
	'wleird',
	'c',
	version: '0.0.0',
	license: 'MIT',
	meson_version: '>=0.43.0',
	default_options: [
		'c_std=c11',
		'warning_level=2',
		'werror=true',
	],
)

add_project_arguments('-Wno-unused-parameter', language: 'c')
add_project_arguments('-Wno-missing-braces', language: 'c')

wleird_inc = include_directories('include')

cairo = dependency('cairo')
wayland_client = dependency('wayland-client')
wayland_server = dependency('wayland-server')
wayland_protos = dependency('wayland-protocols', version: '>=1.14')

subdir('protocol')

wleird_deps = [cairo, client_protos, wayland_client]

lib_client = static_library(
	'client',
	files(
		'client.c',
		'pool-buffer.c',
	),
	include_directories: wleird_inc,
	dependencies: wleird_deps,
)

executable(
	'cursor',
	files('cursor.c'),
	link_with: lib_client,
	include_directories: wleird_inc,
	dependencies: wleird_deps,
	install: true,
)

executable(
	'resizor',
	files('resizor.c'),
	link_with: lib_client,
	include_directories: wleird_inc,
	dependencies: wleird_deps,
	install: true,
)

executable(
	'subsurfaces',
	files('subsurfaces.c'),
	link_with: lib_client,
	include_directories: wleird_inc,
	dependencies: wleird_deps,
	install: true,
)

executable(
	'unmap',
	files('unmap.c'),
	link_with: lib_client,
	include_directories: wleird_inc,
	dependencies: [wleird_deps, wayland_server],
	install: true,
)
