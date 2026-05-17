#include <stdio.h>
#include <string.h>
#include <err.h>


int isAnOption(const char *opt) {
	if (*opt == '-') {
		return (1);
	}
	return (0);
}

int main(int argc, char *argv[]) {
	int mode = -1;
	FILE *fp;

	char *fOption = NULL;
	char *tOption[argc];
	int listFilesCount = 0;

	for (int i = 1; i < argc; ++i) {
		const char *argument = argv[i];

		if (strcmp(argument, "-f") == 0) {
			if (i + 1 < argc) {
				fOption = argv[i + 1];
				++i;
			}
			else {
				printf("mytar: Must specify filename parameter\n");
				return (1);
			}
		}
		else if (strcmp(argument, "-t") == 0) {
			mode = 3;
			if (i + 1 < argc) {
				if (isAnOption(argv[i + i])) {
					continue;
				}
				++i;
				while (i < argc) {
					tOption[listFilesCount] = argv[i];
					++i;
					++listFilesCount;
				}
			}
		}
		else {
			printf("mytar: Unknown option %s\n", argument);
			return (1);
		}
	}

	if (mode == -1) {
		printf("mytar: Must specify one of the following: -t\n");
		return (1);
	}

	if (mode == 3) {
		if (fOption == NULL) {
			printf("mytar: Must specify filename parameter\n");
			return (1);
		}

		if ((fp = fopen(fOption, "r")) == NULL) {
			err(1, "fopen");
		}
	}

	fclose(fp);
	return (0);
}
