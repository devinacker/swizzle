/*
	swizzle - reorder address & data bits in a ROM image
	
	See COPYING.txt for license information.
*/

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <math.h>

#define VERSION "1.0.0"

// ----------------------------------------------------------------------------
static void usage()
{
	fprintf(stderr,
	"usage: swizzle [options] in_path out_path\n"
	"\n"
	"supported options:\n"
	"  -h / --help             show this information and exit\n"
	"  -a / --addr <bits>      specify address bit order (optional)\n"
	"  -d / --data <bits>      specify data bit order (optional)\n"
	"  -w / --word <num>       specify number of bytes per word (default 1, max 4)\n"
	"  -b / --big              use big-endian byte ordering\n"
	"\n"
	"<bits> is a comma-separated list of 0-based bit indexes\n"
	"  (comma separated, most significant first).\n"
	"\n"
	"Example: to reverse the order of bits in each byte:\n"
	"  swizzle -d0,1,2,3,4,5,6,7 in.bin out.bin\n"
	);
	
	exit(1);
}

// ----------------------------------------------------------------------------
static void error(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);
	exit(1);
}

// ----------------------------------------------------------------------------
static void parseBits(char *bits, int *dest, int count, const char *type)
{
	int pos = count;
	unsigned long bitsFound = 0;
	
	char *bit = strtok(bits, ",");
	while (bit)
	{
		const int index = atoi(bit);
		if (pos > 0)
			dest[pos - 1] = index;
		
		if (index < 0 || index >= count)
			error("invalid %s bit index %d (must be between 0 and %d)\n", type, index, count - 1);
		
		if (bitsFound & (1 << index))
			fprintf(stderr, "warning: %s bit index %d specified multiple times\n", type, index);
		bitsFound |= (1 << index);
		
		bit = strtok(NULL, ",");
		pos--;
	}
	
	if (pos != 0)
	{
		error("expected %d %s bits, but %d were specified\n", count, type, count - pos);
	}
}

// ----------------------------------------------------------------------------
static unsigned long swizzleWord(unsigned long word, const int *bits, int count)
{
	long out = 0;
	
	for (int i = 0; i < count; i++)
	{
		if (bits[i] >= i)
			out |= (word & (1 << bits[i])) >> (bits[i] - i);
		else
			out |= (word & (1 << bits[i])) << (i - bits[i]);
	}
	return out;
}

typedef struct 
{
	const char *inPath;
	const char *outPath;
	char *addrBits;
	char *dataBits;
	int bytesPerWord;
	int bigEndian;
} swizzle_t;

// ----------------------------------------------------------------------------
static void swizzle(const swizzle_t *opts)
{
	FILE *inFile = fopen(opts->inPath, "rb");
	if (!inFile)
	{
		error("unable to open %s for reading\n", opts->inPath);
	}
	FILE *outFile = fopen(opts->outPath, "wb");
	if (!outFile)
	{
		error("unable to open %s for writing\n", opts->outPath);
	}
	
	fseek(inFile, 0, SEEK_END);
	long fileSize;
	long inSize = fileSize = ftell(inFile);
	if (fileSize < 0)
	{
		error("error getting size of %s\n", opts->inPath);
	}
	
	if (opts->addrBits && (inSize & (inSize - 1)))
	{
		// if we're swizzling address bits, then size must be a power of two
		fprintf(stderr, "warning: non-power-of-two input size (%ld bytes)\n", inSize);
		inSize |= inSize >> 1;
		inSize |= inSize >> 2;
		inSize |= inSize >> 4;
		inSize |= inSize >> 8;
		inSize |= inSize >> 16;
		inSize++;
	}
	if (inSize % opts->bytesPerWord)
	{
		fprintf(stderr, "warning: input size is not a multiple of %d bytes\n", opts->bytesPerWord);
		inSize += (opts->bytesPerWord - 1);
		inSize &= ~(opts->bytesPerWord - 1);
	}
	
	const int numAddrBits = (int)log2(inSize / opts->bytesPerWord);
	const int numDataBits = 8 * opts->bytesPerWord;
	
	if (numAddrBits < 1 || numAddrBits > 32)
		error("address bus width must be between 1 and 32 bits\n");

	// parse the user-supplied address and data bit ordering
	int addrBits[32] = {0};
	int dataBits[32] = {0};
	
	if (opts->addrBits)
		parseBits(opts->addrBits, addrBits, numAddrBits, "address");
	if (opts->dataBits)
		parseBits(opts->dataBits, dataBits, numDataBits, "data");

	// now do the actual transformation
	unsigned char *inData = calloc(inSize, 1);
	unsigned char *outData = calloc(inSize, 1);
	if (!inData || !outData)
		error("unable to allocate %d bytes\n", inSize * 2);
	
	fseek(inFile, 0, SEEK_SET);
	if (fread(inData, 1, fileSize, inFile) != (size_t)fileSize)
		error("unable to read %d bytes from %s\n", fileSize, opts->inPath);
	
	for (long pos = 0; pos < inSize; pos += opts->bytesPerWord)
	{
		long outAddr = pos;
		if (opts->addrBits)
			outAddr = opts->bytesPerWord * swizzleWord(pos / opts->bytesPerWord, addrBits, numAddrBits);
		
		unsigned long data = 0;
		if (opts->bigEndian)
			for (int i = 0; i < opts->bytesPerWord; i++)
				data |= (inData[pos + i] << (8 * (opts->bytesPerWord - i - 1)));
		else
			for (int i = 0; i < opts->bytesPerWord; i++)
				data |= (inData[pos + i] << (8 * i));
		
		if (opts->dataBits)
			data = swizzleWord(data, dataBits, numDataBits);
		
		if (opts->bigEndian)
			for (int i = 0; i < opts->bytesPerWord; i++)
				outData[outAddr + i] = data >> (8 * (opts->bytesPerWord - i - 1));
		else
			for (int i = 0; i < opts->bytesPerWord; i++)
				outData[outAddr + i] = data >> (8 * i);
	}
	
	if (fwrite(outData, 1, inSize, outFile) != (size_t)inSize)
		error("unable to write %d bytes to %s\n", inSize, opts->outPath);

	free(inData);
	free(outData);
	fclose(inFile);
	fclose(outFile);
}

static const struct option options[] = 
{
	{"help", 0, NULL, 'h'},
	{"addr", 1, NULL, 'a'},
	{"data", 1, NULL, 'd'},
	{"word", 1, NULL, 'w'},
	{"big",  0, NULL, 'b'},
	{0}
};

// ----------------------------------------------------------------------------
int main(int argc, char **argv)
{
	printf("swizzle v" VERSION " (" __DATE__ " " __TIME__")\n");

	swizzle_t opts;
	opts.inPath = opts.outPath = NULL;
	opts.addrBits = opts.dataBits = NULL;
	opts.bytesPerWord = 1;
	opts.bigEndian = 0;
	
	char opt;
	while ((opt = getopt_long(argc, argv, ":ha:d:w:b", options, NULL)) != -1)
	{
		switch (opt)
		{
		case ':':
		case 'h':
			usage();
			break;
		
		case 'a':
			opts.addrBits = optarg;
			break;
			
		case 'd':
			opts.dataBits = optarg;
			break;
		
		case 'w':
			opts.bytesPerWord = atoi(optarg);
			if (opts.bytesPerWord < 1 || opts.bytesPerWord > 4)
			{
				fprintf(stderr, "bytes per word must be between 1-4\n");
				exit(-1);
			}
			break;
		
		case 'b':
			opts.bigEndian = 1;
			break;
		}
	}
	
	if ((optind + 1) >= argc)
		usage();
	
	opts.inPath = argv[optind];
	opts.outPath = argv[optind + 1];
	
	swizzle(&opts);
	return 0;
}
