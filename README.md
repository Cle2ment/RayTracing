# Ray Tracing
![Free Palestine](https://freepalestinemovement.org/wp-content/uploads/2013/06/banner.jpg)
![Static Badge](https://img.shields.io/badge/Inspired_by-TheCherno-yellow?logo=Github)
![Static Badge](https://img.shields.io/badge/Language-C++-blue?logo=cplusplus)
![Static Badge](https://img.shields.io/badge/Built_by-Premake-blue?logo=lua)
![Static Badge](https://img.shields.io/badge/License-MIT-green)


## Description
This is a Ray Tracing repository built by C++.

## How To USE
`NOTICE: You need to have Vulkan SDK installed on your PC first to run the project.`
1. To clone the repository, use `git clone https://github.com/BoningtonChen/RayTracing.git`
2. Run `Setup.bat` in the `scripts`folder.
3.  Open `RayTracing.slnx` in your Visual Studio, and run the project.(Recommend running in Release or Dist mode to retrieve better performance).

## Demonstration
The Ray Tracing project is still under development. \
Here is the current demonstration of the project.\
![Ray Tracing Default Example](https://github.com/BoningtonChen/RayTracing/blob/master/Materials/RayTracing-example01.png)
![Ray Tracing Example](https://github.com/BoningtonChen/RayTracing/blob/master/Materials/RayTracing-example02.png)

## About WalnutAppTemplate
- Description\
This is a simple app template for Walnut - unlike the example within the Walnut repository, this keeps Walnut as an external submodule and is much more sensible for actually building applications. See the Walnut repository for more details.
- Getting Started\
Once you've cloned, you can customize the `premake5.lua` and `WalnutApp/premake5.lua` files to your liking (eg. change the name from "WalnutApp" to something else). Once you're happy, run `scripts/Setup.bat` to generate Visual Studio 2022 solution/project files. Your app is located in the `WalnutApp/` directory, which some basic example code to get you going in `WalnutApp/src/WalnutApp.cpp`. I recommend modifying that WalnutApp project to create your own application, as everything should be setup and ready to go.

## LICENSE
The project uses `MIT License`.

## Copyright
© Bonity, 2024
