# Purism Viewer

Purism Core bundles a sample model viewer built using raylib in
`src/samples/viewer/`. It is by no means complete, lacking physics and a proper
JSON parser; rather it is intended as a demonstration of how to use Purism Core
and implement the more advanced features such as offscreen rendering and v6
blend modes.

## Usage

```
usage: viewer [options] <model.model3.json | model.moc3>
  --shot <file>          render a few frames, save PNG, exit
  --zoom <f>             (with --shot) zoom multiplier, recenter on face
  --nomask               start with masking disabled
  --flat                 debug: bypass offscreen compositing (flat path)
  --novsync              uncap frame rate (measure true ceiling)
  --maskscale <f>        mask buffer resolution fraction (default 0.5)
  --setparam NAME VAL    force a parameter each frame (repeatable)
  --hide N               suppress drawable N (repeatable)
  --only N               draw only drawable N
  --maxorder N           draw only drawables with renderOrder <= N
interactive: Tab panel  M masking  R reset view  wheel zoom  LMB pan
             drag a .model3.json or .moc3 onto the window to load it
```
