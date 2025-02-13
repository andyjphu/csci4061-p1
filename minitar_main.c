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
    char *archive = argv[3];

    for (int i = 4; i < argc; i++) {
        file_list_add(&files, argv[i]);

    }

    if (strcmp(cmd, "-c") == 0) {
        create_archive(archive, &files);
    } else if (strcmp(cmd, "-a") == 0) {
        append_files_to_archive(archive, &files);
    } else if (strcmp(cmd, "-t") == 0) {
        get_archive_file_list(archive, &files);
        node_t *curr = files.head;
        for (int i = 0; i < files.size; i++) {
            printf("%s\n", curr->name);
            curr = curr->next;
        }
    } else if (strcmp(cmd, "-u") == 0) {
        // check if they already exist
        update_archive(archive, &files);
    } else if (strcmp(cmd, "-x") == 0) {
        extract_files_from_archive(archive);
    } else {
        printf("Unknown command: %s\n", cmd);
        return -1;
    }

    file_list_clear(&files);
    return 0;
}
