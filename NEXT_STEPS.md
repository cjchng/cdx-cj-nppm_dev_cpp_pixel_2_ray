# Next Steps

Current project:
- `/home/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray`
- Remote: `https://github.com/cjchng/cdx-cj-nppm_dev_cpp_pixel_2_ray`
- Branch: `master`

Last confirmed good remote checkpoint:
- Commit: `f97b910f015832565ffca15ded17360db22eb577`
- Summary: initial Qt/OpenGL sphere-frame prototype is pushed and visible on GitHub

Current status:
- Qt6/CMake project builds on the user's machine after Qt dependencies were installed
- The app launches successfully when run under a clean environment
- The OpenGL viewport is working and the user confirmed the result looked good
- Core math and interaction helper tests are present in `tests/`

Known runtime note:
- On this machine, normal shell launch picked up an incompatible Snap runtime
- Safe launch command:

```bash
cd /home/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray/build
env -i HOME="$HOME" USER="$USER" PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin" DISPLAY="$DISPLAY" XAUTHORITY="$XAUTHORITY" ./pixel_2_ray_qt
```

Recommended next feature:
1. Add orthographic orbit/pan camera controls while preserving handle drag behavior.

Suggested resume prompt:

```text
Continue from /home/cj/Documents/cdx-cj-nppm_dev_cpp_pixel_2_ray.
Use branch master.
Last good checkpoint: f97b910.
Read NEXT_STEPS.md and continue with orbit/pan camera controls.
```
