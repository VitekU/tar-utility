#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <err.h>
#include <stdlib.h>

#define LIST_MODE 't'
#define EXTRACT_MODE 'x'
#define VERBOSE 'v'

#define HEADER_SIZE 512
#define MAGIC "ustar"
#define MAGIC_LENGTH 5

int isAnOption(const char *opt) {
	if (*opt == '-') {
		return (1);
	}
	return (0);
}

// returns whether the array contains the string and marks the string as present in the "contains[]" array
int isInFilesToList(char **fileList, int fileCount, const char *file, int contains[]) {
	for (int i = 0; i < fileCount; ++i) {
		if (strcmp(fileList[i], file) == 0) {
			contains[i] = 1;
			return (1);
		}
	}
	return (0);
}

long parseOctal(const char *oct, int arraySizeLen) {
	long size = 0;
	int base = 1;
	for (int i = arraySizeLen - 2; i >= 0; --i) {
		size += base * (oct[i] - '0');
		base *= 8;
	}
	return (size);
}

unsigned long long parse256(const char *number, int arraySizeLen) {
	unsigned long long size = 0;
	unsigned long long base = 1;

	for (int i = arraySizeLen - 1; i  >= 4; --i) {
		size += base * number[i];
		base *= 256;
	}
	return (size);
}


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

typedef struct {
	char mode;
	char *fileName;
	char **filesToList;
	int filesToListCount;
	char verbose;

} Args;

void loadOptions(int argc, char **argv, Args *args) {
	args->filesToListCount = 0;
	args->filesToList = malloc(argc * sizeof(char *));
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
		else if (strcmp(argument, "-t") == 0) {
			args->mode = LIST_MODE;
			shouldLoadListItems = 1;
		}
		else if (strcmp(argument, "-x") == 0) {
			args->mode = EXTRACT_MODE;
			shouldLoadListItems = 1;
		}
		else if (strcmp(argument, "-v") == 0) {
			args->verbose = VERBOSE;
		}
		else if (shouldLoadListItems) {
			if (isAnOption(argument)) {
				shouldLoadListItems = 0;
				continue;
			}
			args->filesToList[args->filesToListCount] = argv[i];
			++args->filesToListCount;
		}
		else {
			errx(2, "Unknown option: %s", argument);
		}
	}
}

void list(Args *args) {
	FILE *fp = NULL;
	const Header zeroHeader = {0};

	if (args->fileName == NULL) {
		errx(2, "Missing the -f option");
	}

	if ((fp = fopen(args->fileName, "r")) == NULL) {
		errx(2, "Error opening archive: Failed to open '%s' ", args->fileName);
	}

	Header header;
	int blockCount = 0;
	int archiveContainsFile[args->filesToListCount];
	int allFilesFound = 1;

	for (int i = 0; i < args->filesToListCount; ++i) {
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

		if (header.typeflag != '0' && header.typeflag != '\0') {
			errx(2, "Unsupported header type: %d", header.typeflag);
		}

		long size;
		if (header.size[0] & 0x80) {
			size = parse256(header.size, sizeof(header.size));
		}
		else {
			size = parseOctal(header.size, sizeof(header.size));
		}

		long sizePadded = size;
		if (size % HEADER_SIZE > 0) {
			sizePadded = size + HEADER_SIZE - (size % HEADER_SIZE);
		}

		if (args->filesToListCount == 0) {
			printf("%s\n", header.name);
		}
		else if (isInFilesToList(args->filesToList, args->filesToListCount, header.name, archiveContainsFile)) {
			printf("%s\n", header.name);
		}
		blockCount += sizePadded / HEADER_SIZE;

		if (fseek(fp, sizePadded, SEEK_CUR) == -1) {
			warnx("Unexpected EOF in archive");
			errx(2, "Error is not recoverable: exiting now");
		};
	}

	for (int i = 0; i < args->filesToListCount; ++i) {
		if (archiveContainsFile[i] == 0) {
			allFilesFound = 0;
			warnx("%s: Not found in archive", args->filesToList[i]);
		}
	}

	if (!allFilesFound) {
		errx(2, "Exiting with failure status due to previous errors");
	}
	fclose(fp);
}

void extract(Args *args) {
	FILE *fp = NULL;
	const Header zeroHeader = {0};

	if (args->fileName == NULL) {
		errx(2, "Missing the -f option");
	}

	if ((fp = fopen(args->fileName, "r")) == NULL) {
		errx(2, "Error opening archive: Failed to open '%s' ", args->fileName);
	}

	Header header;
	int blockCount = 0;
	int archiveContainsFile[args->filesToListCount];
	int allFilesFound = 1;

	for (int i = 0; i < args->filesToListCount; ++i) {
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

		if (strncmp(header.magic, MAGIC, MAGIC_LENGTH) != 0) {
			warnx("This does not loook like a tar archive");
			errx(2, "Exiting with failure status due to previous erros");
		}

		if (header.typeflag != '0' && header.typeflag != '\0') {
			errx(2, "Unsupported header type: %d", header.typeflag);
		}

		long size;
		if (header.size[0] & 0x80) {
			size = parse256(header.size, sizeof(header.size));
		}
		else {
			size = parseOctal(header.size, sizeof(header.size));
		}

		long sizePadded = size;
		if (size % HEADER_SIZE > 0) {
			sizePadded = size + HEADER_SIZE - (size % HEADER_SIZE);
		}

		if (args->filesToListCount > 0 && !isInFilesToList(args->filesToList, args->filesToListCount, header.name, archiveContainsFile)) {
			if (fseek(fp, sizePadded, SEEK_CUR) == -1) {
				warnx("Unexpected EOF in archive");
				errx(2, "Error is not recoverable: exiting now");
			};
		}
		else {
			if (args->verbose) {
				printf("%s\n", header.name);
			}
			FILE *fpNew = fopen(header.name, "w");
			char c;
			for (int i = 0; i < size; ++i) {
				if (fread(&c, sizeof(c), 1, fp) == 0) {
					warnx("Unexpected EOF in archive");
					errx(2, "Error is not recoverable: exiting now");
				}
				if (fwrite(&c, 1, sizeof(c), fpNew) == 0) {
					errx(2, "Error writing to file");
				}
			}
			fclose(fpNew);

			if (fseek(fp, sizePadded - size, SEEK_CUR) == -1) {
				warnx("Unexpected EOF in archive");
				errx(2, "Error is not recoverable: exiting now");
			};

		}
		blockCount += sizePadded / HEADER_SIZE;
	}

	for (int i = 0; i < args->filesToListCount; ++i) {
		if (archiveContainsFile[i] == 0) {
			allFilesFound = 0;
			warnx("%s: Not found in archive", args->filesToList[i]);
		}
	}

	if (!allFilesFound) {
		errx(2, "Exiting with failure status due to previous errors");
	}
	fclose(fp);
}

int main(int argc, char *argv[]) {
	Args *args = malloc(sizeof(Args));
	loadOptions(argc, argv, args);

	if (args->mode == LIST_MODE) {
		list(args);
	}
	else if (args->mode == EXTRACT_MODE) {
		extract(args);
	}
	else {
		errx(2, "Must specify one of the following: -t, -x");
	}

	free(args);
	return (0);
}
