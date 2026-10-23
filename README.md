# FOF

A very small C compiler written to demonstrate usage of the [FOB](https://github.com/notjoshki/fob.git) compiler backend API.

The compiler itself is about as barebones as you can get for C. It's moreso just an example for how FOB can interact with a compiler frontend.

## Installation

Firstly, make sure to install the following dependencies:

- gcc
- nasm
- make

Then, clone the repository and run the makefile:

```console
$ git clone https://github.com/notjoshki/fof.git
$ cd fof
$ make
```

## Usage

```console
$ fof [options...] <filename>
```

| Option | Description |
| --- | --- |
| ```--help``` | Show help information |
| ```--version``` | Show the compiler version |
| ```-c``` | Output an object file |
| ```-ir``` | Output an IR file |
| ```-o <filename>``` | Specify the output filename |
| ```-O{0,1,2}``` | Set the optimization level (default=2) |
| ```-S``` | Output an assembly file |

## License

FOF is distributed under the [MIT](./LICENSE) license.
