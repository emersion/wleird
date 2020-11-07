project(
	'wleird',
	'c',
	version: '0.0.0',
	license: 'MIT',
	meson_version: '>=0.47.0',
	default_options: [
		'c_std=c11',
		'warning_level=2',
		'werror=true',
	],
)

cc = meson.get_compiler('c')

add_project_arguments('-Wno-unused-parameter', language: 'c')
add_project_arguments('-Wno-missing-braces', language: 'c')

wleird_inc = include_directories('include')

cairo = dependency('cairo')
wayland_client = dependency('wayland-client')
wayland_server = dependency('wayland-server')
wayland_protos = dependency('wayland-protocols', version: '>=1.14')
math = cc.find_library('m', required: false)

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

clients = {
	'copy-fu': {
		'src': 'copy-fu.c',
		'deps': [math],
	},
	'cursor': {
		'src': 'cursor.c',
	},
	'damage-paint': {
		'src': 'damage-paint.c',
		'deps': [math],
	},
	'disobey-resize': {
		'src': 'disobey-resize.c',
		'deps': [math],
	},
	'frame-callback': {
		'src': 'frame-callback.c',
	},
	'resizor': {
		'src': 'resizor.c',
	},
	'slow-ack-configure': {
		'src': 'slow-ack-configure.c',
	},
	'subsurfaces': {
		'src': 'subsurfaces.c',
	},
	'surface-outputs': {
		'src': 'surface-outputs.c',
	},
	'unmap': {
		'src': 'unmap.c',
		'deps': [wayland_server],
	},
}

foreach name, client : clients
	executable(
		name,
		files(client['src']),
		link_with: lib_client,
		include_directories: wleird_inc,
		dependencies: [wleird_deps] + client.get('deps', []),
		install: true,
	)
endforeach
