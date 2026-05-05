# Agents

## Compiling

Before compiling, set `PKG_CONFIG_PATH` so that the bundled NEML2 install is discoverable:

```bash
export PKG_CONFIG_PATH=$PWD/contrib/neml2/installed/moose/share/pkgconfig:$PKG_CONFIG_PATH
```

Run this from the repository root prior to invoking the build.
