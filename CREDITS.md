# Credits

Pitfall: The Modified Adventure was developed as a collaborative compatibility
and reverse-engineering project.

## Project lead

**`mistermorcus`** directed the project, supplied and tested a
legally owned retail copy, performed extensive gameplay and hardware testing,
identified sound and rumble events, evaluated frame pacing, and made the
project’s design and tuning decisions.

## Engineering assistance

**OpenAI Codex** provided collaborative engineering assistance, including:

- analysis of the Windows 95 executable and legacy display behavior;
- implementation and debugging of the modern framebuffer and Direct3D path;
- frame-pacing investigation and exclusive-fullscreen support;
- local INI persistence, UI extensions, XInput, and rumble integration;
- file/CD music handling and audio troubleshooting;
- DLL loader, build system, installer, uninstaller, and release tooling; and
- technical documentation and release preparation.

Codex is credited as an AI engineering assistant, not as a legal copyright
holder or independent maintainer.

## Third-party work

**Alessandro Ramos** created the MIT-licensed replacement WAIL implementation
that formed the basis of this project’s sound-effects library. The original
copyright notice and full MIT license are retained under
`third_party/pitfall-win11-wail/`.

