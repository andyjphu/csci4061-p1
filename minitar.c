#include "minitar.h"

#include <fcntl.h>
#include <grp.h>
#include <math.h>
#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/sysmacros.h> removed for version issue
#include <sys/types.h>
#include <unistd.h>

#define NUM_TRAILING_BLOCKS 2
#define MAX_MSG_LEN 128
#define BLOCK_SIZE 512

// Constants for tar compatibility information
#define MAGIC "ustar"

// Constants to represent different file types
// We'll only use regular files in this project
#define REGTYPE '0'
#define DIRTYPE '5'

/*
 * Helper function to compute the checksum of a tar header block
 * Performs a simple sum over all bytes in the header in accordance with POSIX
 * standard for tar file structure.
 */
void compute_checksum(tar_header *header) {
    // Have to initially set header's checksum to "all blanks"
    memset(header->chksum, ' ', 8);
    unsigned sum = 0;
    char *bytes = (char *) header;
    for (int i = 0; i < sizeof(tar_header); i++) {
        sum += bytes[i];
    }
    snprintf(header->chksum, 8, "%07o", sum);
}

/*
 * Populates a tar header block pointed to by 'header' with metadata about
 * the file identified by 'file_name'.
 * Returns 0 on success or -1 if an error occurs
 */
int fill_tar_header(tar_header *header, const char *file_name) {
    memset(header, 0, sizeof(tar_header));
    char err_msg[MAX_MSG_LEN];
    struct stat stat_buf;
    // stat is a system call to inspect file metadata
    if (stat(file_name, &stat_buf) != 0) {
        snprintf(err_msg, MAX_MSG_LEN, "Failed to stat file %s", file_name);
        perror(err_msg);
        return -1;
    }

    strncpy(header->name, file_name, 100);    // Name of the file, null-terminated string
    snprintf(header->mode, 8, "%07o",
             stat_buf.st_mode & 07777);    // Permissions for file, 0-padded octal

    snprintf(header->uid, 8, "%07o", stat_buf.st_uid);    // Owner ID of the file, 0-padded octal
    struct passwd *pwd = getpwuid(stat_buf.st_uid);       // Look up name corresponding to owner ID
    if (pwd == NULL) {
        snprintf(err_msg, MAX_MSG_LEN, "Failed to look up owner name of file %s", file_name);
        perror(err_msg);
        return -1;
    }
    strncpy(header->uname, pwd->pw_name, 32);    // Owner name of the file, null-terminated string

    snprintf(header->gid, 8, "%07o", stat_buf.st_gid);    // Group ID of the file, 0-padded octal
    struct group *grp = getgrgid(stat_buf.st_gid);        // Look up name corresponding to group ID
    if (grp == NULL) {
        snprintf(err_msg, MAX_MSG_LEN, "Failed to look up group name of file %s", file_name);
        perror(err_msg);
        return -1;
    }
    strncpy(header->gname, grp->gr_name, 32);    // Group name of the file, null-terminated string

    snprintf(header->size, 12, "%011o",
             (unsigned) stat_buf.st_size);    // File size, 0-padded octal
    snprintf(header->mtime, 12, "%011o",
             (unsigned) stat_buf.st_mtime);    // Modification time, 0-padded octal
    header->typeflag = REGTYPE;                // File type, always regular file in this project
    strncpy(header->magic, MAGIC, 6);          // Special, standardized sequence of bytes
    memcpy(header->version, "00", 2);          // A bit weird, sidesteps null termination
    snprintf(header->devmajor, 8, "%07o",
             major(stat_buf.st_dev));    // Major device number, 0-padded octal
    snprintf(header->devminor, 8, "%07o",
             minor(stat_buf.st_dev));    // Minor device number, 0-padded octal

    compute_checksum(header);
    return 0;
}

/*
 * Removes 'nbytes' bytes from the file identified by 'file_name'
 * Returns 0 upon success, -1 upon error
 * Note: This function uses lower-level I/O syscalls (not stdio), which we'll learn about later
 */
int remove_trailing_bytes(const char *file_name, size_t nbytes) {
    char err_msg[MAX_MSG_LEN];

    struct stat stat_buf;
    if (stat(file_name, &stat_buf) != 0) {
        snprintf(err_msg, MAX_MSG_LEN, "Failed to stat file %s", file_name);
        perror(err_msg);
        return -1;
    }

    off_t file_size = stat_buf.st_size;
    if (nbytes > file_size) {
        file_size = 0;
    } else {
        file_size -= nbytes;
    }

    if (truncate(file_name, file_size) != 0) {
        snprintf(err_msg, MAX_MSG_LEN, "Failed to truncate file %s", file_name);
        perror(err_msg);
        return -1;
    }
    return 0;
}


int create_archive(const char *archive_name, const file_list_t *files) {
    FILE *archive = fopen(archive_name, "wb");
    if (!archive) {
        perror("Failed to open archive file");
        return -1;
    }

    // Curr iterates through file list and write each file to archive
    node_t *curr = files->head;
    while (curr != NULL) {
        if (write_file_to_archive(archive, curr->name) == -1) {
            fclose(archive);
            return -1;
        }
        curr = curr->next;
    }

    // Write trailing two-block footer
    write_footer(archive);

    fclose(archive);
    return 0;
}


int append_files_to_archive(const char *archive_name, const file_list_t *files) {
    struct stat st;
    if (stat(archive_name, &st) != 0) {
        perror("Error: Archive %s doesn't exist");
        return -1;
    }

    if(remove_trailing_bytes(archive_name, BLOCK_SIZE * NUM_TRAILING_BLOCKS) == -1) {
        perror("Error: Couldn't remove trailing bytes");
        return -1;
    }

    FILE *archive = fopen(archive_name, "ab+");
    if (!archive) {
        perror("Error: Failed to open archive");
        return 1;
    }

    node_t *curr = files->head;
    while (curr) {
        if (write_file_to_archive(archive, curr->name) != 0) {
            perror("Error: Failed to append %s to archive.\n");
        }
        curr = curr->next;
    }

    if(write_tar_footer(archive) != 0) {
        perror("Error: Failed to write footer.");
    }

    fclose(archive);
    return 0;
}

int update_archive(const char *archive_name, const file_list_t *files) {
    struct stat st;
    // if (stat(archive_name, &st) != 0) {
    //     printf("Error: Archive '%s' does not exist.\n", archive_name);
    //     return 1;
    // }

    file_list_t new_files;
    file_list_init(&new_files);

    node_t *curr = files->head;
    while (curr) {
        if (!file_exists_in_archive(archive_name, curr->name)) {
            file_list_add(&new_files, curr->name);
        }
        curr = curr->next;
    }

    if (new_files.size > 0) {
        int result = append_files_to_archive(archive_name, &new_files);
        file_list_clear(&new_files);
        return result;
    }

    file_list_clear(&new_files);
    return 0;
}

int get_archive_file_list(const char *archive_name, file_list_t *files) {
    // TODO: Not yet implemented
    return 0;
}

int extract_files_from_archive(const char *archive_name) {
    // TODO: Not yet implemented
    return 0;
}


int file_exists(const char *archive_name, const char *file_name) {
    FILE *archive = fopen(archive_name, "rb");
    if (!archive) return 0; //File does not exist -- return something else?

    char name[MAX_NAME_LEN];
    while (read_tar_header(archive, name, NULL) == 0) {
        if (strcmp(name, file_name) == 0) {
            fclose(archive);
            return -1;
        }
    }

    fclose(archive);
    return 0;
}
