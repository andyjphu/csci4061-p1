#include <stdio.h>
#include <string.h>

#include "file_list.h"
#include "minitar.h"

int main(int argc, char **argv) {
    if (argc < 4) {
        printf("Usage: %s -c|a|t|u|x -f ARCHIVE [FILE...]\n", argv[0]);
        return 0;
    }

    file_list_t files;
    file_list_init(&files);

    char *cmd = argv[1];
    char *archive = argv[2];

    for (int i = 3; i < argc; i++) {
        file_list_add(&files, argv[i]);

    }

    if (strcmp(cmd, "-c") == 0) {
        create_archive(archive, &files);
    } else if (strcomp(cmd, "-a") == 0) {
        append_files_to_archive(archive, &files);
    } else if (strcomp(cmd, "-a") == 0) {
        get_archive_file_list(archive);
    } else if (strcomp(cmd, "-a") == 0) {
        update_archive(archive, &files);
    } else if (strcomp(cmd, "-a") == 0) {
        extract_files_from_archive(archive);
    } else {
        print("Unknown command: %s\n", cmd);
        return -1;
    }

    file_list_clear(&files);
    return 0;
}
