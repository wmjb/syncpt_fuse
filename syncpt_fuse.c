#define FUSE_USE_VERSION 26
#define _FILE_OFFSET_BITS 64

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/types.h>
#include <pwd.h>

static const char *redirect_base = "/sys/devices/platform/tegra_grhost/syncpt";
static uid_t fuse_uid;
static gid_t fuse_gid;

static int syncpt_getattr(const char *path, struct stat *stbuf) {
    memset(stbuf, 0, sizeof(struct stat));
//    printf("getattr called for path: %s\n", path);
//    fflush(stdout);

    stbuf->st_uid = fuse_uid;
    stbuf->st_gid = fuse_gid;

    if (strcmp(path, "/") == 0 || strcmp(path, "/syncpt") == 0) {
        stbuf->st_mode = S_IFDIR | 0555;
        stbuf->st_nlink = 2;
        return 0;
    }

    if (strncmp(path, "/syncpt/", 8) == 0) {
        char *subpath = strdup(path + 8);
        char *slash = strchr(subpath, '/');

        if (!slash) {
            stbuf->st_mode = S_IFDIR | 0555;
            stbuf->st_nlink = 2;
            free(subpath);
            return 0;
        }

        if (strcmp(slash, "/max") == 0) {
            char redirect_path[512];
            snprintf(redirect_path, sizeof(redirect_path), "%s/%.*s/max", redirect_base, (int)(slash - subpath), subpath);

            if (access(redirect_path, R_OK) == 0) {
                stbuf->st_mode = S_IFREG | 0444;
                stbuf->st_nlink = 1;
                stbuf->st_size = 16;
                free(subpath);
                return 0;
            }
        }

        free(subpath);
    }

    return -ENOENT;
}

static int syncpt_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
    (void) offset;
    (void) fi;

    printf("readdir called for path: %s\n", path);
    fflush(stdout);

    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);

    if (strcmp(path, "/") == 0) {
        filler(buf, "syncpt", NULL, 0);
    } else if (strcmp(path, "/syncpt") == 0) {
        DIR *dir = opendir(redirect_base);
        if (!dir) return -EIO;

        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
                printf("Found syncpt index: %s\n", entry->d_name);
                fflush(stdout);
                filler(buf, entry->d_name, NULL, 0);
            }
        }
        closedir(dir);
    } else if (strncmp(path, "/syncpt/", 8) == 0) {
        filler(buf, "max", NULL, 0);
    }

    return 0;
}

static int syncpt_open(const char *path, struct fuse_file_info *fi) {
    if (strncmp(path, "/syncpt/", 8) != 0 || !strstr(path, "/max"))
        return -ENOENT;

    return 0;
}

static int syncpt_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    if (strncmp(path, "/syncpt/", 8) != 0 || !strstr(path, "/max"))
        return -ENOENT;

    char *subpath = strdup(path + 8);
    char *slash = strchr(subpath, '/');
    if (!slash || strcmp(slash, "/max") != 0) {
        free(subpath);
        return -ENOENT;
    }

    char redirect_path[512];
    snprintf(redirect_path, sizeof(redirect_path), "%s/%.*s/max", redirect_base, (int)(slash - subpath), subpath);

    FILE *fp = fopen(redirect_path, "r");
    if (!fp) {
        free(subpath);
        return -EIO;
    }

    char content[128];
    size_t len = fread(content, 1, sizeof(content) - 1, fp);
    fclose(fp);
    free(subpath);

    if (offset < len) {
        if (offset + size > len)
            size = len - offset;
        memcpy(buf, content + offset, size);
    } else {
        size = 0;
    }

    return size;
}

static const struct fuse_operations syncpt_ops = {
    .getattr = syncpt_getattr,
    .readdir = syncpt_readdir,
    .open    = syncpt_open,
    .read    = syncpt_read,
};

int main(int argc, char *argv[]) {
    struct passwd *pw = getpwuid(getuid());
    fuse_uid = pw->pw_uid;
    fuse_gid = pw->pw_gid;

    printf("Starting FUSE...\n");
    fflush(stdout);

    int ret = fuse_main(argc, argv, &syncpt_ops, NULL);
    printf("fuse_main returned: %d\n", ret);
    return ret;
}

