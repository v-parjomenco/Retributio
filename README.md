# SkyGuard (name subject to change)

A 2D aerial combat indie game built with **C++17** and **SFML 3.0.2**.

## Features
- Smooth player movement with configurable speed/scale via JSON
- ResourceManager for textures and fonts
- Config-driven architecture (C++ + JSON)
- Modular core/entity system

## Project Structure
📦 SFML1
┣ 📂 assets
┃ ┣ 📂 config
┃ ┣ 📂 fonts
┃ ┣ 📂 images
┃ ┗ 📂 sounds
┣ 📂 data
┃ ┗ 📂 definitions
┣ 📂 include
┃ ┣ 📂 core
┃ ┣ 📂 entities
┃ ┣ 📂 graphics
┃ ┣ 📂 third_party
┃ ┗ 📂 utils
┣ 📂 src
┣ 📄 CREDITS.md
┗ 📄 LICENSE.MIT

## Build
1. Install [SFML 3.0.2](https://www.sfml-dev.org/download/sfml/3.0.2/)
2. Open the project in Visual Studio
3. Build and run

## License
MIT License. See [LICENSE.MIT](./third_party/LICENSE.MIT).