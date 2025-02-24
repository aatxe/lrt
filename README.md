Lute [![CI](https://github.com/luau-lang/lute/actions/workflows/ci.yml/badge.svg)](https://github.com/luau-lang/lute/actions/workflows/ci.yml)
====

Lute is a standalone runtime for general-purpose programming in [Luau](https://luau.org), and a collection of optional extension libraries for Luau embedders to include to expand the capabilities of the Luau scripts in their software.
It is designed to make it readily feasible to use Luau to write any sort of general-purpose programs, including manipulating files, making network requests, opening sockets, and even making tooling that directly manipulates Luau scripts.
Lute also features a standard library of Luau code, called `std`, that aims to expose a more featureful standard library for general-purpose programming that we hope can be an interface shared across Luau runtimes.

Lute is still very much a work-in-progress, and should be treated as pre-1.0 software without stability guarantees for its API.
We would love to hear from you about your experiences working with other Luau or Lua runtimes, and about what sort of functionality is needed to best make Luau accessible and productive for general-purpose programming.

### Lute Libraries

The Lute repository fundamentally contains three sets of libraries. These are as follows:
- `lute`: The core runtime libraries in C++, which provides the basic functionality for general-purpose Luau programming.
- `std`: The standard library, which extends those core C++ libraries with additional functionality in Luau.
- `batteries`: A collection of useful, standalone Luau libraries that do not depend on `lute`.

Contributions to any of these libraries are welcome, and we encourage you to open issues or pull requests if you have any feedback or contributions to make.
