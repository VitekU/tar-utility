#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <err.h>
#include <stdlib.h>

#define LIST_MODE 't'
#define EXTRACT_MODE 'x'
#define VERBOSE 'v'

#define FILE_OP "-f"
#define LIST_OP "-t"
#define EXTRACT_OP "-x"
#define VERBOSE_OP "-v"

#define HEADER_SIZE 512
#define CORRECT_MAGIC "ustar"
#define MAGIC_LENGTH 5
#define CORRECT_TYPEFLAG '0'
#define MSB_MASK 0x80 // mask to determine the value of the most significant bit of an 8 bit number
#define CHUNK_SIZE 8192 // size of the chunk that is being read in an extract mode


/*
 * the Args struct that is being passed to the functions that process the individual modes
 */
typedef struct {
	char mode;
	char *fileName;
	char **filesToProcess;
	int filesToProcessCount;
	char verbose;

} Args;

/*
 * the tar header struct according to the specification
 */
typedef struct {
	char name[100];
	char mode[8];
	char uid[8];
	char gid[8];
	char size[12];
	char mtime[12];
	char chksum[8];
	char typeflag;
	char linkname[100];
	char magic[6];
	char version[2];
	char uname[32];
	char gname[32];
	char devmajor[8];
	char devminor[8];
	char prefix[155];
	char padding[12];
} Header;


int isAnOption(const char *op) {
	if (*op == '-') {
		return (1);
	}
	return (0);
}

long getFileSize(FILE *fp) {
	fseek(fp, 0, SEEK_END);
	long size = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	return (size);
}

/*
 * checks if the *file string is in the **fileList array
 * if present it marks the string as present in the *contains array
 */
int isInfilesToProcess(char **fileList, int fileCount, const char *file, int *contains) {
	for (int i = 0; i < fileCount; ++i) {
		if (strcmp(fileList[i], file) == 0) {
			contains[i] = 1;
			return (1);
		}
	}
	return (0);
}

/*
 * returns the decimal value of the file size in octal encoding
 */
long parseOctal(const char *oct, int arraySizeLen) {
	long size = 0;
	int base = 1;
	for (int i = arraySizeLen - 2; i >= 0; --i) {
		size += base * (oct[i] - '0');
		base *= 8;
	}
	return (size);
}

/*
 * returns the decimal value of the file size in base 256 encoding
 */
unsigned long long parse256(const char *number, int arraySizeLen) {
	unsigned long long size = 0;
	unsigned long long base = 1;

	for (int i = arraySizeLen - 1; i  >= 4; --i) {
		size += base * number[i];
		base *= 256;
	}
	return (size);
}

int isTarHeader(Header *header) {
	if (strncmp(header->magic, CORRECT_MAGIC, MAGIC_LENGTH) == 0) {
		return (1);
	}
	return (0);
}


/*
 * takes a header and returns the file size in decimal
 */
long getSizeDecimal(Header *header) {
	if (header->size[0] & MSB_MASK) {
		return(parse256(header->size, sizeof(header->size)));
	}
	else {
		return(parseOctal(header->size, sizeof(header->size)));
	}
}

/*
 * returns the padded size
 */
long getSizePadded(long size) {
	if (size % HEADER_SIZE > 0) {
		return(size + HEADER_SIZE - (size % HEADER_SIZE));
	}
	return size;
}

/*
 * creates and validates the file pointer *fp
 */
FILE* createFilePtr(Args *args) {
	FILE *fp = NULL;
	if (args->fileName == NULL) {
		errx(2, "Missing the -f option");
	}

	if ((fp = fopen(args->fileName, "r")) == NULL) {
		errx(2, "Error opening archive: Failed to open '%s' ", args->fileName);
	}
	return fp;
}

/*
 * validates the typeflag and magic of the header
 */
void validateHeader(Header *header) {
	if (strncmp(header->magic, CORRECT_MAGIC, MAGIC_LENGTH) != 0) {
		warnx("This does not look like a tar archive");
		errx(2, "Exiting with failure status due to previous errors");
	}
	if (header->typeflag != CORRECT_TYPEFLAG && header->typeflag != '\0') {
		errx(2, "Unsupported header type: %d", header->typeflag);
	}
}

/*
 * one by one reads and writes n characters from a file to another file
 */
void readAndWriteRange(int n, FILE *fp, FILE *fpNew) {
	char c;
	for (int i = 0; i < n; ++i) {
		if (fread(&c, sizeof(c), 1, fp) != 1) {
			warnx("Unexpected EOF in archive");
			errx(2, "Error is not recoverable: exiting now");
		}
		if (fwrite(&c, sizeof(c), 1, fpNew) != 1) {
			errx(2, "Error writing to file");
		}
	}
}

/*
 * parses the **argv arguments into the *args struct
 */
void loadOptions(int argc, char **argv, Args *args) {
	args->filesToProcessCount = 0;
	args->filesToProcess = malloc(argc * sizeof(char *));
	int shouldLoadListItems = 0;
	for (int i = 1; i < argc; ++i) {
		const char *argument = argv[i];

		if (strcmp(argument, "-f") == 0) {
			if (i + 1 < argc) {
				args->fileName = argv[i + 1];
				++i;
			}
			else {
				errx(2, "Option -f requires an argument");
			}
		}
		else if (strcmp(argument, LIST_OP) == 0) {
			args->mode = LIST_MODE;
			shouldLoadListItems = 1;
		}
		else if (strcmp(argument, EXTRACT_OP) == 0) {
			args->mode = EXTRACT_MODE;
			shouldLoadListItems = 1;
		}
		else if (strcmp(argument, VERBOSE_OP) == 0) {
			args->verbose = VERBOSE;
		}
		else if (shouldLoadListItems) {
			if (isAnOption(argument)) {
				shouldLoadListItems = 0;
				continue;
			}
			args->filesToProcess[args->filesToProcessCount] = argv[i];
			++args->filesToProcessCount;
		}
		else {
			errx(2, "Unknown option: %s", argument);
		}
	}
}

/*
 * handles the LIST_MODE mode
 */
void list(Args *args, FILE *fp) {
	const Header zeroHeader = {0};
	Header header;
	long fileSize = getFileSize(fp);
	int blockCount = 0;
	int *archiveContainsFile = NULL;
	if (args->filesToProcessCount > 0) {
		archiveContainsFile = calloc(args->filesToProcessCount, sizeof(int));
	}

	/*
	 * when specifying the files to be listed, we need to ensure that the archive does in fact contain them
	 * by default we assume so and thus the value is set to true
	 */
	int allFilesFound = 1;

	while (fread(&header, HEADER_SIZE, 1, fp) == 1) {
		++blockCount;
		if (memcmp(&header, &zeroHeader, HEADER_SIZE) == 0) {
			if (fread(&header, HEADER_SIZE, 1, fp) == 1 && memcmp(&header, &zeroHeader, HEADER_SIZE) == 0) {
				break;
			}
			warnx("A lone zero block at %d", blockCount);
			break;
		}

		validateHeader(&header);

		long size = getSizeDecimal(&header);
		long sizePadded = getSizePadded(size);

		if (args->filesToProcessCount == 0) {
			printf("%s\n", header.name);
		}
		else if (isInfilesToProcess(args->filesToProcess, args->filesToProcessCount, header.name, archiveContainsFile)) {
			printf("%s\n", header.name);
		}
		blockCount += sizePadded / HEADER_SIZE;

		if (ftell(fp) + sizePadded > fileSize) {
			warnx("Unexpected EOF in archive");
			errx(2, "Error is not recoverable: exiting now");
		}
		fseek(fp, sizePadded, SEEK_CUR);
	}

	for (int i = 0; i < args->filesToProcessCount; ++i) {
		if (archiveContainsFile[i] == 0) {
			allFilesFound = 0;
			warnx("%s: Not found in archive", args->filesToProcess[i]);
		}
	}

	if (!allFilesFound) {
		errx(2, "Exiting with failure status due to previous errors");
	}
}

/*
 * handles the EXTRACT_MODE mode
 */
void extract(Args *args, FILE *fp) {
	const Header zeroHeader = {0};
	Header header;
	long fileSize = getFileSize(fp);
	int blockCount = 0;
	int *archiveContainsFile = NULL;
	if (args->filesToProcessCount > 0) {
		archiveContainsFile = calloc(args->filesToProcessCount, sizeof(int));
	}

	/*
	 * when specifying the files to be extracted, we need to ensure that the archive does in fact contain them
	 * by default we assume so and thus the value is set to true
	 */

	int allFilesFound = 1;

	for (int i = 0; i < args->filesToProcessCount; ++i) {
		archiveContainsFile[i] = 0;
	}

	while (fread(&header, HEADER_SIZE, 1, fp) == 1) {
		++blockCount;
		if (memcmp(&header, &zeroHeader, HEADER_SIZE) == 0) {
			if (fread(&header, HEADER_SIZE, 1, fp) == 1 && memcmp(&header, &zeroHeader, HEADER_SIZE) == 0) {
				break;
			}
			warnx("A lone zero block at %d", blockCount);
			break;
		}

		validateHeader(&header);

		long size = getSizeDecimal(&header);
		long sizePadded = getSizePadded(size);

		if (args->filesToProcessCount > 0 && !isInfilesToProcess(args->filesToProcess, args->filesToProcessCount, header.name, archiveContainsFile)) {
			if (ftell(fp) + sizePadded > fileSize) {
				warnx("Unexpected EOF in archive");
				errx(2, "Error is not recoverable: exiting now");
			}
			fseek(fp, sizePadded, SEEK_CUR);
		}
		else {
			if (args->verbose) {
				printf("%s\n", header.name);
			}
			FILE *fpNew = fopen(header.name, "w");

			int chunksToRead = size / CHUNK_SIZE;
			int remainderToRead = size % CHUNK_SIZE;

			char chunk[CHUNK_SIZE];

			for (int i = 0; i < chunksToRead; ++i) {
				if (ftell(fp) + CHUNK_SIZE > fileSize) {
					readAndWriteRange(fileSize - ftell(fp), fp, fpNew);
					warnx("Unexpected EOF in archive");
					errx(2, "Error is not recoverable: exiting now");
				}
				fread(&chunk, sizeof(chunk), 1, fp);
				if (fwrite(&chunk, sizeof(chunk), 1, fpNew) != 1) {
					errx(2, "Error writing to file");
				}
			}

			readAndWriteRange(remainderToRead, fp, fpNew);
			fclose(fpNew);

			if (ftell(fp) + sizePadded - size > fileSize) {
				warnx("Unexpected EOF in archive");
				errx(2, "Error is not recoverable: exiting now");
			}
			fseek(fp, sizePadded - size, SEEK_CUR);

		}
		blockCount += sizePadded / HEADER_SIZE;
	}

	for (int i = 0; i < args->filesToProcessCount; ++i) {
		if (archiveContainsFile[i] == 0) {
			allFilesFound = 0;
			warnx("%s: Not found in archive", args->filesToProcess[i]);
		}
	}

	if (!allFilesFound) {
		errx(2, "Exiting with failure status due to previous errors");
	}
}

int main(int argc, char *argv[]) {
	Args *args = calloc(1, sizeof(Args));
	loadOptions(argc, argv, args);
	FILE *fp = createFilePtr(args);

	if (args->mode == LIST_MODE) {
		list(args, fp);
	}
	else if (args->mode == EXTRACT_MODE) {
		extract(args, fp);
	}
	else {
		errx(2, "Must specify one of the following: -t, -x");
	}

	fclose(fp);
	free(args);
	return (0);
}
