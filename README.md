`swizzle` is a utility for reordering (swizzling) the address and/or data bits of a ROM image.

Usage: `swizzle [options] in_path out_path`

Supported options:
* `-h` / `--help` - show this information and exit
* `-a` / `--addr <bits>` - specify address bit order (optional)
* `-d` / `--data <bits>` - specify data bit order (optional)
* `-w` / `--word <num>` - specify number of bytes per word (default 1, max 4)
* `-b` / `--big` - use big-endian byte ordering

`<bits>` is a comma-separated list of 0-based bit indexes (comma separated, most significant first).

Example: to reverse the order of bits in each byte:

`swizzle -d0,1,2,3,4,5,6,7 in.bin out.bin`
