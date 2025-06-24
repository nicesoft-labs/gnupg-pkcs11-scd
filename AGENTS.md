To build the project locally, install the required build dependencies:
- `build-essential` (for compiler and tools)
- `pkg-config`
- `libp11-kit-dev`
- `libassuan-dev`
- `libksba-dev`
- `libgcrypt20-dev`
- `libgtk-3-dev` or other optional libs depending on environment
- `libssl-dev` if OpenSSL is desired, but this build is configured without it
- `libtool`, `autoconf`, and `automake`

Configure the project using:

```sh
./configure --prefix=/usr --without-openssl
```

Then build and run tests using:

```sh
make
make check
```
