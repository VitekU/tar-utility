#include <stdio.h>
#include <string.h>
#include <err.h>


int isAnOption(const char *opt) {
	if (*opt == '-') {
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

int isInFilesToList(char **fileList, int fileCount, const char* file) {
	for (int i = 0; i < fileCount; ++i) {
		if (strcmp(fileList[i], file) == 0) {
			return (1);
		}
	}
	return (0);
}

struct Header {
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
};

int main(int argc, char *argv[]) {
	int mode = -1;
	FILE *fp = NULL;

	char *archiveFileName = NULL;
	char *filesToList[argc];
	int filesToListCount = 0;

	const struct Header zeroHeader = {0};

	for (int i = 1; i < argc; ++i) {
		const char *argument = argv[i];

		if (strcmp(argument, "-f") == 0) {
			if (i + 1 < argc) {
				archiveFileName = argv[i + 1];
				++i;
			}
			else {
				errx(1, "Option -f requires an argument");
			}
		}
		else if (strcmp(argument, "-t") == 0) {
			mode = 3;
			if (i + 1 < argc) {
				if (isAnOption(argv[i + 1])) {
					continue;
				}
				++i;
				while (i < argc) {
					filesToList[filesToListCount] = argv[i];
					++i;
					++filesToListCount;
				}
			}
		}
		else {
			errx(1, "Unknown option: %s", argument);
		}
	}

	if (mode == -1) {
		errx(1, "Must specify one of the following: -t");
	}

	if (mode == 3) {
		if (archiveFileName == NULL) {
			errx(1, "Missing the -f option");
		}

		if ((fp = fopen(archiveFileName, "r")) == NULL) {
			errx(1, "Error opening archive: Failed to open '%s' ", archiveFileName);
		}
		struct Header header;
		long fileSize = getFileSize(fp);
		int blockCount = 0;

		while (fread(&header, sizeof(header), 1, fp) == 1) {
			++blockCount;
			if (memcmp(&header, &zeroHeader, 512) == 0) {
				if (fread(&header, sizeof(header), 1, fp) == 1 && memcmp(&header, &zeroHeader, 512) == 0) {
					break;
				}
				warnx("A lone block at %d", blockCount - 1);
				break;
			}

			int size = 0;
			int base = 1;
			for (int i = sizeof(header.size) - 2; i >= 0; --i) {
				size += base * (header.size[i] - '0');
				base *= 8;
			}

			int sizePadded = size;
			if (size % 512 > 0) {
				sizePadded = size + 512 - (size % 512);
			}

			if (filesToListCount == 0) {
				printf("%s\n", header.name);
			}
			else if (isInFilesToList(filesToList, filesToListCount, header.name)) {
				printf("%s\n", header.name);
			}
			blockCount += sizePadded / 512;

			if (ftell(fp) + sizePadded > fileSize) {
				fprintf(stderr, "mytar: Unexpected EOF in archive\n");
				errx(1, "Error is not recoverable: exiting now");
			}
			fseek(fp, sizePadded, SEEK_CUR);
		}
	}

	fclose(fp);
	return (0);
}
