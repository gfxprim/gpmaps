# PIA file format specification

PIA file format is used for storing a large number of image tiles in one file.
PIA format is not an image format, it is a container format (similar to TAR,
CPIO ... formats). It can contain arbitrary image types (usually PNG and JPEG).
Images form a rectangular mesh, every stored image is indexed by coordinates.
It is supposed that stored images have the same file format and dimensions.

PIA format consists of three parts - header, table and data areas. The data
area contains many item areas. PIA format uses a little endian to store
numbers. In the description we will use `uint32_t` for a litte endian 32bit
unsigned integer and `uint64_t` for a litte endian 64bit unsigned integer.

The header area is at the beginning of the file, it is described by this
C structure:

```c
struct pia_header
{
	uint32_t magic;
	uint32_t version;
	uint32_t table_width;
	uint32_t table_height;
	uint32_t tile_width;
	uint32_t tile_height;
	uint32_t empty_color;
	uint32_t extended_block_size; //was reserved in version 0
	char suffix[8];
};
```

- PIA header magic is 0x59A14C76

- The current version is 1, The program should fail to open file when the
  version of the file is greater than 1.

- `table_width` and `table_height` describe width and height
  of a mesh of images. Therefore, each stored image is indexed
  by [X, Y] coordinates, where 0 <= X < `table_width`, 0 <= Y < `table_height`.

- `tile_width` and `tile_height` describe pixel dimensions of each stored image.

- `empty_color` describes a color that should be used when a tile is missing.

- suffix describes an original suffix of the files (jpeg, png, ...), padded
  from right by zeroes.

- `extended_block_size` is size of extended block that immediatelly follows
  after table.

- Reserved areas should be written with zeroes

The extended block starts with header:

```c
struct pia_extended_block_header //only in version 1
{
	uint32_t type;
	uint32_t shortlen; //only for types of blocks that are smaller than 2G
}
```

Version 1 supports only one type of extended block:
0x72646548 (Little endian "Hedr") ... This chunk has to be added before all version1 chunks

The table area is stored immediately after the header area. It is an array of
these C structures:

```c
struct pia_node
{
	uint64_t offset;
	uint64_t size; //in version 1 is upper bit (i.e., 1 << 63) set for version1 items
};
```

Each structure describes one item area (one stored image). offset specifies
offset of an item area (from the beginning of the file) and size represents
a size of a data part of an item area (a size of an item area without
its header) including common prefix (pia version 1 extension).

The array of `pia_node` structs represents an index table of tiles. The number
of nodes is `tile_width * tile_height`, The index of [X, Y]-node is `X + (Y *
table_width)`.

If a tile is missing, the offset and size si zero. In that case, solid
rectangle of `empty_color` should be used instead of that.

The data area is an area immediately after the table area to the end of the
file. It is just a space to store item areas. It is possible that same space
in the data area is not used - for example when a tile is replaced, the new
tile can be appended to the end of file and the space used by the old tile
is unused now. The table area specifies what space is used and how.

The item area is a space when one file is stored. It has a header part and a
data part. The header part starts at the position specified in `pia_node`
structure in the table area, the header part size is fixed. The data part
starts immediately after the header part and its size is specified in `pia_node`
structure in the table area. The header part is described by this C structure:

```c
struct pia_version0_item_header
{
	uint32_t magic;
	uint32_t reserved;
	uint32_t x;
	uint32_t y;
	uint64_t size;
};
```

- PIA version0 item header magic is 0x97F21E5B

- x, y, and size is redundant with the other values (size from `pia_node`
  structure and x, y, from the position in the table area. They are stored
  here in order to recovery of stored image in a case of broken (or missing)
  the header area or the table area

Version 1 items have size of whole image in `pia_node`, but data part of version1
item is shorter by common data header. Thus size in header of the block is also
smaller by this value (but nonnegative).

```c
struct pia_version1_item_header
{
	uint32_t magic;
	uint32_t size;
};
```

- PIA version1 item header magic is 0x4d455469 (little endian 'iTEM')

- Version1 item MUST be smaller than 2GB

- More table entries MAY point to same version1 item

The data part contains the stored image data file.
