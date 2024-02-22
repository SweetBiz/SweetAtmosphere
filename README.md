# SweetAtmosphere
Precomputed Atmospheric (Single) Scattering for Unreal Engine.
Looks good from within and outside of the atmosphere.

Based on Schafhitzel, Tobias et al. “Real-Time Rendering of Planets with Atmospheres.” J. WSCG 15 (2007): 91-98.

![Banner](https://github.com/Sweet-Biz/SweetAtmosphere/assets/10288753/f0259a79-3c99-47a5-9c06-159a1ec32760)

https://github.com/Sweet-Biz/SweetAtmosphere/assets/10288753/a79a2cdb-afbd-4181-ad68-f2330f24910b

## Usage
The recommended way to install `SweetAtmosphere` is by adding this repository as a git submodule to your project's `Plugins` folder.

Please examine the `PrecomputedAtmosphereDemo` Level in the Plugin's `Content` folder for a demo.

The Event Graph of `BP_ExamplePlanetActor` shows the basic setup, which consists of precomputing textures
and binding them to the atmosphere material.

It is recommended to apply the atmosphere material to a cube with inverted normals. Such a mesh is supplied in the plugin content.

## License
This project is GPL-3.0 licensed. Please make sure you comply with the license terms when using this library.
