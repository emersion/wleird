# wleird

A collection of Wayland clients doing weird things, for compositor testing.

Some of these clients are intentionally misusing protocols in ways that help
reveal compositor behavior, and should not be seen as reference implementations
for good Wayland clients.

```shell
meson build
ninja -C build
```

* `attach-delta-loop`: uses buffer position to move itself around in a circle
* `copy-fu`: implements a number of copy-paste behaviors
* `cursor`: uses buffer position to update a cursor's hotspot
* `damage-paint`: uses fine-grained damage requests to draw shapes
* `disobey-resize`: submits buffers in a different size than configured
* `frame-callback`: requests frame callbacks indefinitely
* `resize-loop`: resizes itself indefinitely
* `resizor`: uses buffer position to initiate a client-side resize
* `slow-ack-configure`: responds to configure events very slowly
* `subsurfaces`: displays a bunch of subsurfaces and lets you reorder them
* `surface-outputs`: prints on which outputs a surface is on
* `unmap`: unmaps a buffer after displaying it

## License

MIT
