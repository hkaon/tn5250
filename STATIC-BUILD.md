# Building tn5250 as a Fully Static Binary (musl + OpenSSL)

This guide produces a self-contained static binary with no runtime library dependencies, using musl libc and statically linked OpenSSL.

## Prerequisites

```bash
sudo apt install musl-tools musl-dev build-essential wget
```

## Step 1: Build ncurses with musl

```bash
cd /tmp
wget https://ftp.gnu.org/gnu/ncurses/ncurses-6.4.tar.gz
tar xzf ncurses-6.4.tar.gz
cd ncurses-6.4

CC=musl-gcc ./configure \
    --prefix=/opt/musl-ncurses \
    --without-shared \
    --without-debug \
    --without-cxx \
    --without-cxx-binding \
    --without-ada \
    --without-tests \
    --without-progs \
    --disable-stripping

make -j$(nproc)
make install
```

## Step 2: Build OpenSSL with musl

```bash
cd /tmp
wget https://github.com/openssl/openssl/releases/download/openssl-3.0.13/openssl-3.0.13.tar.gz
tar xzf openssl-3.0.13.tar.gz
cd openssl-3.0.13

CC=musl-gcc CFLAGS="-idirafter /usr/include -idirafter /usr/include/x86_64-linux-gnu" \
    ./Configure linux-x86_64 \
    no-shared \
    no-async \
    no-dso \
    --prefix=/opt/musl-openssl \
    --openssldir=/opt/musl-openssl/ssl

make -j$(nproc)
make install_sw
```

> **Note:** The `-idirafter` flags allow OpenSSL to find Linux kernel headers
> without conflicting with musl's own headers. The `no-async` and `no-dso`
> options avoid features that don't work well with static musl builds.

## Step 3: Configure and compile tn5250

```bash
cd /path/to/tn5250-0.18.0

CC=musl-gcc \
CFLAGS="-I/opt/musl-ncurses/include/ncurses -I/opt/musl-ncurses/include -I/opt/musl-openssl/include" \
LDFLAGS="-static -L/opt/musl-ncurses/lib -L/opt/musl-openssl/lib64" \
LIBS="-lssl -lcrypto -lncurses" \
    ./configure \
    --enable-static \
    --disable-shared \
    --with-ssl=/opt/musl-openssl

make -j$(nproc)
```

> **Note:** OpenSSL may install its static libraries under `lib64/` instead of
> `lib/` depending on the platform. Adjust the `-L` path accordingly.

## Step 4: Link static binaries

Libtool may not pass `-static` through to the final link. To guarantee fully
static binaries, link manually:

```bash
# Main terminal emulator (includes headless mode via --headless flag)
musl-gcc -static -o tn5250 \
    curses/tn5250-cursesterm.o curses/tn5250-headlessterm.o curses/tn5250-tn5250.o \
    lib5250/.libs/lib5250.a \
    -L/opt/musl-openssl/lib64 -L/opt/musl-ncurses/lib \
    -lssl -lcrypto -lncurses -lpthread

# Print server daemon
musl-gcc -static -o lp5250d \
    lp5250d/lp5250d.o lib5250/.libs/lib5250.a \
    -L/opt/musl-openssl/lib64 -L/opt/musl-ncurses/lib \
    -lssl -lcrypto -lncurses

# SCS format converters
for tool in scs2ascii scs2pdf scs2ps; do
    musl-gcc -static -o ${tool} \
        lp5250d/${tool}.o lib5250/.libs/lib5250.a \
        -L/opt/musl-openssl/lib64 -L/opt/musl-ncurses/lib \
        -lssl -lcrypto -lncurses
done
```

## Step 5: Strip (optional)

Remove debug symbols to reduce binary size:

```bash
strip tn5250 lp5250d scs2ascii scs2pdf scs2ps
```

## Verify

```bash
file tn5250
# Expected: ELF 64-bit LSB executable, x86-64, ... statically linked, stripped

ldd tn5250
# Expected: not a dynamic executable
```

## Build without SSL

If SSL/TLS support is not needed, skip Step 2 and replace Step 3 with:

```bash
CC=musl-gcc \
CFLAGS="-I/opt/musl-ncurses/include/ncurses -I/opt/musl-ncurses/include" \
LDFLAGS="-static -L/opt/musl-ncurses/lib" \
LIBS="-lncurses" \
    ./configure \
    --enable-static \
    --disable-shared \
    --with-ssl=no

make -j$(nproc)
```

Then in Step 4, omit the `-lssl -lcrypto` flags and the OpenSSL `-L` path.
This produces much smaller binaries (~140-490K vs ~4-5M).
