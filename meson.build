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
wayland_protos = dependency('wayland-protocols', version: '>=1.14')

subdir('protocol')

executable(
	'client',
	files([
		'pool-buffer.c',
		'client.c',
	]),
	dependencies: [
		cairo,
		client_protos,
		wayland_client,
	],
	include_directories: [wleird_inc],
	install: true,
)
