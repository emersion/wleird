# wleird

A collection a Wayland clients doing weird things, for compositor testing.

```shell
meson build
ninja -C build
```

* `cursor`: uses buffer position to update a cursor's hotspot
* `resizor`: uses buffer position to initiate a client-side resize
* `subsurfaces`: displays a bunch of subsurfaces and lets you reorder them

## License

MIT
