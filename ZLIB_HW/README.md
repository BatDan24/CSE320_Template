# Homework 2 - CSE 320 - Spring 2026
#### Professor Dan Benz

Readme version 0.9, see Piazza for any updates

### **Due Date: Sunday March 8th @ 11:59 PM **

**Read the entire doc before you start**

## Introduction

In the last assignment, you (hopefully) manipulated PNG images, which use the gzip compression format. In this assignment, you will debug and optimize a program to interact with gzip-compressed files. This is traditionally done with a library called zlib.\
The specification for zlib can be found at: [http://tools.ietf.org/html/rfc1951](http://tools.ietf.org/html/rfc1951) for the block format and algorithms, and [http://tools.ietf.org/html/rfc1952](http://tools.ietf.org/html/rfc1952) for the overall file format. Some more helpful specification descriptions can be found at [https://zlib.net](https://zlib.net), particulary [https://zlib.net/feldspar.html](https://zlib.net/feldspar.html) for a more simple description of the algorithms.
The goal of this homework is to familiarize yourself with debugging and profiling C programs,
with a focus on algorithms, data structures in C, buffered I/O,
and memory allocation. Writing C code to compress and decompress files poses a realistic and challenging exercise involving these topics.

> :nerd_face: Reference for pointers: [https://beej.us/guide/bgc/html/#pointers](https://beej.us/guide/bgc/html/#pointers).

For all assignments in this course, you **MUST NOT** put any of the functions
that you write into the `main.c` file.  The file `main.c` **MUST ONLY** contain
`#include`s, local `#define`s and the `main` function.  The reason for this
restriction has to do with our use of the Criterion library to test your code
(this is discussed further at the end of this document).
Beyond this, you may have as many or as few additional `.c` files in the `src`
directory as you wish.  Also, you may declare as many or as few headers as you wish.
Note, however, that header and `.c` files distributed with the assignment base code
often contain a comment at the beginning which states that they are not to be
modified.  **PLEASE** take note of these comments and do not modify any such files,
as they will be replaced by the original versions during grading.

For all the assignments in this course, you are expected to write your own code;
you are not permitted to submit source code that you did not write yourself,
except for any code that is distributed as part of the basecode repository,
or code for which written permission has explicitly been given.
You are also not permitted to use library functions other than those provided
as part of the official Linux Mint software environment for the course.
In addition, for this particular assignment, there are some further restrictions
on what libraries you are allowed to use.

- Allowed:
  - `glibc` (GNU standard C library), including dynamic memory allocation
     (`malloc`, `calloc`) and standard I/O library functions (`fread`, *etc.*).

- Disallowed:
  - Binary libraries other than `glibc`.
  - `zlib` or any other compression/decompression library

Submitting or using code that is not allowed will quite likely result in your
receiving a zero for this assignment and could potentially expose you to
charges of Academic Dishonesty.

> :scream:  Many students will find this to be a relatively challenging project.
> It is extremely unlikely that anyone will be able to just read through the handout
> materials and then sit down and write the code.  You should certainly expect to have to
> experiment with the data files you have been provided, in order to develop and
> validate your understanding of the data formats discussed here.  This experimentation
> will take time, so you need get started on it as soon as possible and allocate
> time regularly over the next few weeks in order to be able to have a working
> implementation for at least part of the specification by the time the due date arrives.

# Getting Started

This semester we will be using codegrade to submit and grade your projects.
If you are reading this document then you should have successfully used codegrade to
create your own git repo of my template linked to codegrade. Codegrade will rerun the
basic test scripts every time you do a git push. 

> :scream: The tests that will be used to determine your grade are NOT the same
> as the tests you will see in this template repository or in your view of codegrade.
> I provide these tests to help you check basic functionality, ensure that your print 
> formatting matches mine, and show you if your codegrade submission compiles. 
> The real test cases will be run after the submission deadline. 
> I STRONGLY encourage you to write your own additional tests to ensure that your code handles 
> all of the required functionality.

Here is the structure of the base code:

<pre>
ZLIB_HW
│   .gitignore
│   create_fixtures.sh
│   Makefile
│   README.md
│
├───include
│       debug.h
│       huff.h
│       lz.h
│       our_zlib.h
│       queue.h
│       utility.h
│
├───src
│       huff.c
│       lz.c
│       main.c
│       queue.c
│       utility.c
│       zlib.c
│
└───tests
        test_huff.c
        test_lz.c
        test_zlib.c
</pre>

- The `.gitignore` file is a file that tells `git` to ignore files with names
matching specified patterns, so that they don't accidentally end up getting
committed to the repository.

> :scream:  The CI testing is for your own information; it does not directly have
> anything to do with assignment grading.
> The purpose of the tests are to alert you to possible problems with your code; 
> if you see that testing has failed it is worth investigating why that has happened.  

- The `Makefile` is a configuration file for the `make` build utility, which is what
you should use to compile your code.  In brief, `make` or `make all` will compile
anything that needs to be, `make debug` does the same except that it compiles the code
with options suitable for debugging, and `make clean` removes files that resulted from
a previous compilation.  These "targets" can be combined; for example, you would use
`make clean debug` to ensure a complete clean and rebuild of everything for debugging.

- The `include` directory contains C header files (with extension `.h`) that are used
by the code.  Note that these files often contain `DO NOT MODIFY` instructions at the beginning.
You should observe these notices carefully where they appear.

- The `src` directory contains C source files (with extension `.c`).

- The `tests` directory contains C source code (and sometimes headers and other files)
that are used by the Criterion tests.

## A Note about Program Output

What a program does and does not print is VERY important.
In the UNIX world stringing together programs with piping and scripting is
commonplace. Although combining programs in this way is extremely powerful, it
means that each program must not print extraneous output. For example, you would
expect `ls` to output a list of files in a directory and nothing else.
Similarly, your program must follow the specifications for normal operation.
One part of our grading of this assignment will be to check whether your program
produces EXACTLY the specified output.  If your program produces output that deviates
from the specifications, even in a minor way, or if it produces extraneous output
that was not part of the specifications, it will adversely impact your grade
in a significant way, so pay close attention.

> :scream: Use the debug macro `debug` for any other program output or messages you many need
> while coding (e.g. debugging output).

# Part 1: Program Operation and Argument Validation

Your program will be a command-line utility which when invoked will open a file, directory, or archive and process queries specified as command-line arguments.
The results of the query processing will be output to another file or standard output.  
In case of successful execution, your program should terminate with exit status `EXIT_SUCCESS` (0).  
In case of any error, your program should print a one-line message describing the error to the standard error output,
and your program should terminate with exit status `EXIT_FAILURE` (1).
Unless the program has been compiled for debugging (using `make debug`),
in a successful run that exits with `EXIT_SUCCESS` no unspecified output may be produced
by the program.

The specific usage scenarios for your program are as follows:

<pre>
Usage: bin/zlib -i file [options]
Options:
  -i file                Input .gz file (required if -h flag is absent)
  -h                     Print this help message
  -m                     Print member summary
  -c [dictionary]        Compress the file
  -d [dictionary]        Decompress the file
</pre>

> :scream: A `PRINT_USAGE` macro has already been provided for you in the `global.h`
> header file.  You should use this macro to print the help message.

Note that the square brackets in the description are not part of what the user types,
but rather are used to indicate that the enclosed arguments are optional.
The `-i` flag is **required** and must be provided for all operations except `-h`.

The program uses a two-pass argument processing approach:

**First Pass**: The program traverses the command-line arguments to:
- Check if `-h` is specified (if so, print usage and exit with `EXIT_SUCCESS`)
- Locate the `-i` flag, extract the associated filename, get the second option, and parse the following arguments

If `-h` is specified, the program prints the usage message to standard output
and exits with status `EXIT_SUCCESS`, ignoring all other arguments.

If `-i` is not specified or is missing its filename argument, the program prints
an error message to standard error and exits with `EXIT_FAILURE`.

**Second Pass**: After successfully validating arguments in the first pass,
the program traverses the command-line arguments again to process the requested operations.
The results of these operations are printed to standard output in the specific formats
described below.  The meaning of the various option flags are as follows:

- `-m` (Member summary): The program prints a member summary for the .gz file, listing all member metadata, including CRC validity status.  The output format is:
```
Member Summary for <file>:
  Member 0: Compression Method: 0, Last Modified: 1770439737, OS: 5, Extra: 15, Comment: the beginning, Size: 13, CRC: valid
  Member image.png: Compression Method: 3, Last Modified: 1770439224, OS: 2, Extra: 0, Size: 63995, CRC: valid
  ...
  Member N: Compression Method: 2, Last Modified: 1770423114, OS: 0, Extra: 0, Comment: the end, Size: 13, CRC: invalid
```

- `-d [dictionary]` (Decompress): The program decompresses a gzip file or archive using the dictionary, if provided.  The output format should be the raw uncompressed bytes.
  If a member has an invalid CRC or Header CRC (HCRC) or parsing fails, an error message
  is printed to standard error.

- `-c [dictionary]` (Compress): The program compresses the given file.  The output format should be the raw compressed bytes.

If any unrecognized options or options with missing arguments are encountered during
the argument processing phase, then the program should print a single line error message
to stderr that describes the issue (using the appropriate error macro from `global.h`)
and then exit with status `EXIT_FAILURE`.

Dictionaries for Huffman coding will be encoded as strings in the following format: `byte_value_1:num_bits_1,byte_value_2:num_bits_2,...` where both byte_value_n and num_bits_n are decimal numbers. You may assume that every byte value occurs at most once, byte values are less than 256, and number of bits values are less than 16.

> :nerd_face: Reference for command line arguments: [https://beej.us/guide/bgc/html/#command-line-arguments](https://beej.us/guide/bgc/html/#command-line-arguments).

The `main` function in `main.c` implements the two-pass argument processing approach:

I have provided macros in `global.h` for all output formatting to ensure consistency.

# Part 2: Huffman Coding (`huff.c`)

Huffman coding is used to losslessly compress data by referring to bytes that occur most frequently with the least number of bits.
Byte values get placed into a binary tree (described later in this section) in such a way that the least frequently occuring bytes end up at the bottom and more frequently occurring bytes towards the top.
The encoding of each byte value is determined like this: starting from the root node, every left branch taken is a 0 and every right branch taken is a 1.
The sequence of 0's and 1's are how the byte will be represented in the encoded data.

All Huffman tree nodes contain a frequency.
A Huffman tree node can either store a byte value or have two children.
```c
typedef struct huff_node {
	struct huff_node* left;
	struct huff_node* right;
	int symbol; /* -1 for internal */
	unsigned long freq;
} huff_node_t;
```

## Huffman Compression and Serialization
`unsigned char* huffman_encode(unsigned char* data, size_t len, size_t* out_len)`

In order to create the tree with these properties, the two least-frequently occurring bytes are treated as leaf nodes which store the byte value and the frequency.
They are connected to a parent, which will contain the sum of their frequencies.
The 'sum' nodes are treated as regular nodes: if there is a sum node that contains the value 20 and the next least-occuring byte value to be added to the tree has a frequency of 21, the sum node and regular node become children of a sum node with a value of 41.\
A rule is added so that a Huffman tree can be consistently constructed in the same way:
all byte values with the same encoded length/at the same level of the tree must be in lexicographical (alphabetical) order 
(e.g. if A and B are children of the same parent, A must be the left child).
This repeats until there is no remaining byte value that is not in the tree (there are a maximum of 256 byte values).
However, information to construct the tree also needs to be sent along with the encoded bytes (serialization will be discussed in detail in Part 4).

## Huffman Decompression
`unsigned char* huffman_decode(unsigned char* data, size_t enc_len, size_t* out_len)`

A Huffman code can be reconstructed from an ordered list of encoding lengths, which are ordered so they correspond to consecutive byte values.
First, the frequency of each encoding length is tabulated.
Next, the encoding for the first byte value in each level is calculated.
Finally, the encodings for the rest of the byte values in each level is calculated by incrementing that of the previous, starting from the second.
The tree is followed according to the byte encoding to retrieve the byte value.

## Helper Functions

- `void build_codes(huff_node_t* node, unsigned long code, int len, unsigned long* codes, unsigned char* lens)`
- `static void free_tree(huff_node_t* n)`
- `static void canonical_codes(unsigned char* lens, unsigned long* codes)`
- `int get_code_length_codes(unsigned int hclen, unsigned char* data, unsigned int *offset, unsigned long* code_length_codes, unsigned int *bit)`
- `int get_literal_length_codes(unsigned int hlit, unsigned char* data, unsigned int* offset, unsigned long code_length_codes[19], unsigned short** literal_length_table, unsigned int* bit)`
- `int get_distance_codes(int hdist, unsigned char* data, unsigned int* offset, unsigned long code_length_codes[19], unsigned short** distance_table, unsigned int* bit)`

# Part 3: Lempel-Ziv (`lz.c`)

Lempel-Ziv compression is used to losslessly compress data by replacing repeated sequences with references to their last occurrence.
Each 'reference' contains an offset and a length component:
The offset is how far back in the decompressed data the repeated string starts (relative to the current position).
The length is used to know how many bytes to copy starting from the offset position.
The max. distance is 32768 and the max. length is 258.

## LZ77 Compression and Serialization
`unsigned char* lz_compress(const unsigned char* data, size_t len, size_t* out_len)`

Your compressor must find the longest matching substring within 32768 previous bytes of the output buffer. The distance-length pairs are encoded into bits using the following table:
```
      Extra                Extra                Extra 
 Code Bits  Length(s) Code Bits  Lengths   Code Bits  Length(s)
 ---- ----  ------     ---- ----  -------   ---- ----  -------
  257   0      3       267   1    15,16     277   4    67-82
  258   0      4       268   1    17,18     278   4    83-98
  259   0      5       269   2    19-22     279   4    99-114
  260   0      6       270   2    23-26     280   4   115-130
  261   0      7       271   2    27-30     281   5   131-162
  262   0      8       272   2    31-34     282   5   163-194
  263   0      9       273   3    35-42     283   5   195-226
  264   0     10       274   3    43-50     284   5   227-257
  265   1   11,12      275   3    51-58     285   0     258
  266   1   13,14      276   3    59-66

      Extra            Extra               Extra 
 Code Bits  Dist  Code Bits   Dist     Code Bits  Distance
 ---- ----  ----  ---- ----  ------    ---- ----  --------
   0   0     1     10   4     33-48    20    9    1025-1536
   1   0     2     11   4     49-64    21    9    1537-2048
   2   0     3     12   5     65-96    22   10    2049-3072
   3   0     4     13   5     97-128   23   10    3073-4096
   4   1    5,6    14   6    129-192   24   11    4097-6144
   5   1    7,8    15   6    193-256   25   11    6145-8192
   6   2    9-12   16   7    257-384   26   12    8193-12288
   7   2   13-16   17   7    385-512   27   12    12289-16384
   8   3   17-24   18   8    513-768   28   13    16385-24576
   9   3   25-32   19   8   769-1024   29   13    24577-32768
```

## LZ77 Decompression
`unsigned char* lz_decompress(const unsigned char* data, size_t comp_len, size_t* out_len)`

It is possible for the referenced string to exceed the end of the sliding window/the offset is less than the length (e.g. a string starting 1 byte ago with a length of 5 is referenced).
In these cases, the bytes are inserted into the output buffer and the decoding process continues with length - 1 and offset - 1.

## Helper Functions
- `static int find_match(const unsigned char* data, size_t pos, size_t len, size_t offset_limit, size_t* out_offset)`
- `unsigned char* lz_to_length_distance_codes(unsigned char* data, size_t len, size_t* out_len)`
- `unsigned char* length_distance_codes_to_lz(unsigned char* data, size_t len, size_t* out_len)`

# Part 4: Putting it all together

*GNU ZIP (gzip) files* are a file format used for lossless data compression.
gzip files are constructed by applying several rounds of two different compression algorithms to encode a file. The compressed file data is surrounded with metadata about the file and the compression method used. The unit of the data and its metadata is called a member.

The first 2 bytes of all members are the same. They are the following values:
0x1f 0x8b.

A bunch of metadata is stored in a member header, but here are the most important ones for this assignment, in the order that they appear:
1. **Compression Method** (1 byte): Either 0 (no compression) or 8 (standard deflate compression algorithm)
2. **Flag** (1 byte): contains information about the presence of optional metadata fields
3. **CRC** (4 bytes): A cyclic redundancy check computed over the uncompressed data, represented as a big-endian unsigned 32-bit integer.
4. **ISIZE** (4 bytes): size of the data before it was compressed

What you have to do for this part of the
assignment is to debug functions for reading a gzip file
from a C input stream.

## gzip Data Structures

The definitions of the type `gz_header_t` is
given in the header file `include/zlib.h`.

```c
typedef struct {
	unsigned char	cm;			// compression method
	unsigned char	flags;
    unsigned int	mtime;		// modification time
    unsigned char	xflags;		// extra flags
    unsigned char	os;			// operating system
    unsigned short	extra_len;	// extra field length							(optional)
    char*			extra;		// pointer to extra field or NULL				(optional)
    char*			name;		// pointer to zero-terminated file name or NULL (optional)
    char*			comment;	// pointer to zero-terminated comment or NULL	(optional)
    unsigned short	hcrc;		// header CRC, 16 bits							(optional)
	unsigned int	crc;		// 32-bit CRC for the data
	unsigned int	full_size;	// uncompressed size
} gz_header_t;

typedef struct {
	2_bits	BTYPE;	// can be 00 (no compression), 01 (fixed Huffman code), or 10 (dynamic Huffman code)
	1_bit	BFINAL;	// 1 if this is the last block
} block_header_t;

typedef struct {
	short len;
	short len_complement;
	char* data;				// limited to 65535 bytes in length
} no_compression_member_t;

typedef struct {
	5_bits HLIT;	//
	5_bits HDIST;	//
	4_bits HCLEN;	// 
} fixed_huffman_member_t;
```

```
* Data elements are packed into bytes in order of
  increasing bit number within the byte, i.e., starting
  with the least-significant bit of the byte.
* Data elements other than Huffman codes are packed
  starting with the least-significant bit of the data
  element.
* Huffman codes are packed starting with the most-
  significant bit of the code.
```
 
When reading bits in the data, a value will either be a literal or a length-offset pair.
The way to distinguish between a length-offset tuple and a literal byte is that literal bytes have a value less than 256. 
These semantics will need to be encoded so the LZ decoder can tell the difference.

The following tables can be used for converting values greater than 256 to the length of the string and values less than 30 to offset.
The defined number of extra bits need to be read and their value (MSB-first order) added to the length/offset.
```
      Extra               Extra               Extra
 Code Bits Length(s) Code Bits Lengths   Code Bits Length(s)
 ---- ---- ------     ---- ---- -------   ---- ---- -------
  257   0     3       267   1   15,16     277   4   67-82
  258   0     4       268   1   17,18     278   4   83-98
  259   0     5       269   2   19-22     279   4   99-114
  260   0     6       270   2   23-26     280   4  115-130
  261   0     7       271   2   27-30     281   5  131-162
  262   0     8       272   2   31-34     282   5  163-194
  263   0     9       273   3   35-42     283   5  195-226
  264   0    10       274   3   43-50     284   5  227-257
  265   1  11,12      275   3   51-58     285   0    258
  266   1  13,14      276   3   59-66

      Extra           Extra               Extra
 Code Bits Dist  Code Bits   Dist     Code Bits Distance
 ---- ---- ----  ---- ----  ------    ---- ---- --------
   0   0    1     10   4     33-48    20    9   1025-1536
   1   0    2     11   4     49-64    21    9   1537-2048
   2   0    3     12   5     65-96    22   10   2049-3072
   3   0    4     13   5     97-128   23   10   3073-4096
   4   1   5,6    14   6    129-192   24   11   4097-6144
   5   1   7,8    15   6    193-256   25   11   6145-8192
   6   2   9-12   16   7    257-384   26   12  8193-12288
   7   2  13-16   17   7    385-512   27   12 12289-16384
   8   3  17-24   18   8    513-768   28   13 16385-24576
   9   3  25-32   19   8   769-1024   29   13 24577-32768
```

### Compression - 'DEFLATE' Algorithm
`char* deflate(char* bytes, size_t len, char* dictionary)`

The standard states:
```
compressor terminates a block when it determines that starting a
new block with fresh trees would be useful, or when the block size
fills up the compressor's block buffer.
```
For simplicity and consistency during testing, compressed blocks produced by your code should not exceed 65535 bytes, the same restriction placed on uncompressed blocks.
> However, for decompression, your program cannot assume that all non-final compressed blocks will all have 65535 bytes nor be limited to 65535 bytes in length.

### Decompression - 'INFLATE' Algorithm
`char* inflate(char* bytes, size_t comp_len, size_t dec_len, char* dictionary)`

Each 'block' of compressed data begins with three header **bits** (NOT bytes).
The first, `BFINAL` is 1 if and only if this block is the last block.
The next two, `BTYPE` contains information about the compression mode of the following data.
```
00 - no compression
01 - compressed with fixed Huffman codes
10 - compressed with dynamic Huffman codes
11 - reserved (error)
```

In the case of no compression (00), the rest of the first byte is ignored. The following two bytes are the length of the data in the block.
This is followed by the one's complement of the previous two bytes, followed by the data.

In the case that fixed Huffman codes (01) are used:
Byte values less than 256 are converted to their corresponding byte value in the tree.
If the value 256 is read, this is the end of the Huffman coding.
Otherwise, read the following offset value (may be from a previous block!) and use the encoded value there to get the next byte.

```
Lit Value    Bits        Codes
---------    ----        -----
  0 - 143     8          00110000 through
                         10111111
144 - 255     9          110010000 through
                         111111111
256 - 279     7          0000000 through
                         0010111
280 - 287     8          11000000 through
                         11000111
```

In the case that dynamic Huffman codes (10) are used, this block will contain the data of a Huffman tree (which was used to compress the LZ compression of the data).
The first Huffman tree stores the length parts of the tuple.
The first 5 **bits** (`HLIT`) store how many *more* literal/length codes than 257 there will be\
The next 5 **bits** (`HDIST`) store how many *more* distance codes than 1 there will be\
The next 4 **bits** (`HCLEN`) store how many *more* code length codes than 4 there will be\
The next `HCLEN`+4 **groups** of 3 **bits** store code lengths for the numbers 0-18 (the code length alphabet) in the following order:
```
16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
```
(statistically most common lengths values first).
```
0 - 15: Represent code lengths of 0 - 15 (0 means the corresponding byte value will not be in the Huffman tree)
    16: Copy the previous code length 3 - 6 times.
        The next 2 bits indicate repeat length
              (0 = 3, ... , 3 = 6)
           Example:  Codes 8, 16 (+2 bits 11),
                     16 (+2 bits 10) will expand to
                     12 code lengths of 8 (1 + 6 + 5)
    17: Repeat a code length of 0 for 3 - 10 times.
        (3 bits of length)
    18: Repeat a code length of 0 for 11 - 138 times
        (7 bits of length)
```
For example, 101 110 001 000 100 (spaces addded for clarity) means that
16 is encoded with 5 bits, 17 with 6 bits, 18 with 1 bit, 0 is not present in the data at all, and 8 is encoded with 4 bits (assume `HCLEN` was 1).
The next `HLIT`+257 Huffman codes are for literal byte values\
The next `HDIST`+1 code lengths are for the distance alphabet\
This is followed by the data, which is terminated by a 256 value which symbolizes the end of the block.

## gzip Member Parsing Function (`zlib.c`)

The `zlib.c` module provides functions for parsing gzip member data into
structured formats:

### `int parse_member(FILE* file, gz_header_t* header)`

Parses member into a structured format. The function extracts the compression method, flags, last modified time, flags, operating system information, extra data, name, comment (if any), CRC, and uncompressed size from the
member data. All multi-byte integers are stored in little-endian format.

**Parameters:**
- `file`: Pointer to a file with a file pointer pointing to the start of member data
- `header`: Pointer to output structure where parsed data will be written

**Returns:**
- `0` on success
- `-1` on error (NULL pointers, failure to read from file)

### `char* inflate(char* bytes, size_t comp_len, char* dictionary)`

**Parameters:**
- `bytes`: Pointer to compressed data
- `comp_len`: Length of compressed data
- `dictionary`: an array where element `i` contains the number of bits used to encode byte value `i`
<!-- ! MIGHT RUN INTO ISSUES HERE WITH TWO LAYERS OF ENCODING -->

**Returns:**
- `0` on success
- `-1` on error (NULL pointers, malformed headers)

**Memory Management:** The caller is responsible for freeing the allocated array using `free()`.

### `char* deflate(char* bytes, size_t len, char* dictionary)`

**Parameters:**
- `bytes`: Pointer to data
- `len`: Number of bytes of compressed data
- `dictionary`: an array where element `i` contains the number of bits used to encode byte value `i`

**Returns:**
- `0` on success
- `-1` on error (NULL pointers, malformed headers)

**Memory Management:** The caller is responsible for freeing the allocated array using `free()`.