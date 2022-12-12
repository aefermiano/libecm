# libecm

Error Code Modeler

Original Author: Neill Corlett

Fork of ecm-tools (bin2ecm and ecm2bin) transformed to a library so it can easily be integrated with other applications.

Library interface returns periodically so it does not need to block a thread during the processing.

# Building

It has no external dependencies, so simply:

```
cmake .
cmake --build .
```

You can also install both the libraries and bin2ecm and ecm2bin:

```
sudo make install
```

Although original code supports a variety of systems and compilers, the build files were built for and only tested on GCC for GNU/Linux 64-bits.

# Usage of the library

Inspect these files to learn how to use the library:

```
examples/ecm2bin.c
examples/bin2ecm.c
```

# Usage of the example tools

They mimic the original bin2ecm and ecm2bin, but using the library for processing.

##### ECMify

```
bin2ecm foo.bin
bin2ecm foo.bin bar.bin.ecm
```

##### UnECMify
```
ecm2bin foo.bin.ecm
ecm2bin foo.bin.ecm bar.bin
```
