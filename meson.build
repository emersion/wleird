project(
	'wleird',
	'c',
	version: '0.0.0',
	license: 'MIT',
	meson_version: '>=0.49.0',
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
gbm = dependency('gbm', disabler: true)

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
	'attach-delta-loop': {
		'src': 'attach-delta-loop.c',
		'deps': [math],
	},
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
	'gamma-blend': {
		'src': 'gamma-blend.c',
	},
	'resize-loop': {
		'src': 'resize-loop.c',
	},
	'resizor': {
		'src': 'resizor.c',
	},
	'resource-thief': {
		'src': 'resource-thief.c',
		'deps': [gbm],
	},
	'sigbus': {
		'src': 'sigbus.c',
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
		'wleird-' + name,
		files(client['src']),
		link_with: lib_client,
		include_directories: wleird_inc,
		dependencies: [wleird_deps] + client.get('deps', []),
		install: true,
	)
endforeach
