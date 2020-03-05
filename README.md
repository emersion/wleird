# wleird

A collection a Wayland clients doing weird things, for compositor testing.

```shell
meson build
ninja -C build
```

* `copy-fu`: implements a number of copy-paste behaviors
* `cursor`: uses buffer position to update a cursor's hotspot
* `damage-paint`: uses fine-grained damage requests to draw shapes
* `frame-callback`: requests frame callbacks indefinitely
* `resizor`: uses buffer position to initiate a client-side resize
* `subsurfaces`: displays a bunch of subsurfaces and lets you reorder them
* `unmap`: unmaps a buffer after displaying it

## License

MIT
