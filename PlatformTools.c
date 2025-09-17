// platform.c

#include "include/lua-5.4.8/src/lua.h"
#include "include/lua-5.4.8/src/lauxlib.h"
#include "include/lua-5.4.8/src/lualib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>

// 1. Read file
static int platform_readFile(lua_State *L) {
    const char *path = luaL_checkstring(L, 1);
    FILE *f = fopen(path, "rb");
    if (!f) return luaL_error(L, "Could not open file");

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    char *buffer = malloc(size + 1);
    if (!buffer) {
        fclose(f);
        return luaL_error(L, "Memory error");
    }

    fread(buffer, 1, size, f);
    buffer[size] = '\0';
    fclose(f);

    lua_pushlstring(L, buffer, size);
    free(buffer);
    return 1;
}

// 2. Write file
static int platform_writeFile(lua_State *L) {
    const char *path = luaL_checkstring(L, 1);
    size_t len;
    const char *data = luaL_checklstring(L, 2, &len);

    FILE *f = fopen(path, "wb");
    if (!f) return luaL_error(L, "Could not open file for writing");

    fwrite(data, 1, len, f);
    fclose(f);
    lua_pushboolean(L, 1);
    return 1;
}

// 3. Copy file
static int platform_copyFile(lua_State *L) {
    const char *src = luaL_checkstring(L, 1);
    const char *dst = luaL_checkstring(L, 2);

    FILE *f1 = fopen(src, "rb");
    if (!f1) return luaL_error(L, "Could not open source file");

    FILE *f2 = fopen(dst, "wb");
    if (!f2) {
        fclose(f1);
        return luaL_error(L, "Could not open destination file");
    }

    char buffer[4096];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), f1)) > 0) {
        fwrite(buffer, 1, bytes, f2);
    }

    fclose(f1);
    fclose(f2);
    lua_pushboolean(L, 1);
    return 1;
}

// 4. Create symlink
static int platform_createJunction(lua_State *L) {
    const char *target = luaL_checkstring(L, 1);
    const char *linkPath = luaL_checkstring(L, 2);

    if (symlink(target, linkPath) != 0) {
        return luaL_error(L, "Failed to create symlink");
    }

    lua_pushboolean(L, 1);
    return 1;
}

// 5. HTTP GET
static int parse_url(const char *url, char **host, char **path) {
    const char *p;
    if (strncmp(url, "http://", 7) != 0) return -1; // Only support http

    p = url + 7;
    const char *slash = strchr(p, '/');
    if (!slash) {
        *host = strdup(p);
        *path = strdup("/");
    } else {
        *host = strndup(p, slash - p);
        *path = strdup(slash);
    }
    return 0;
}

static char *http_get(const char *host, const char *path) {
    struct addrinfo hints = {0}, *res;
    int sockfd = -1;
    char request[512];
    char *response = NULL;
    size_t response_size = 0;

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(host, "80", &hints, &res) != 0)
        return NULL;

    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd < 0) {
        freeaddrinfo(res);
        return NULL;
    }

    if (connect(sockfd, res->ai_addr, res->ai_addrlen) != 0) {
        close(sockfd);
        freeaddrinfo(res);
        return NULL;
    }

    freeaddrinfo(res);

    snprintf(request, sizeof(request),
             "GET %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n",
             path, host);

    if (write(sockfd, request, strlen(request)) < 0) {
        close(sockfd);
        return NULL;
    }

    char buffer[1024];
    ssize_t n;
    while ((n = read(sockfd, buffer, sizeof(buffer))) > 0) {
        char *tmp = realloc(response, response_size + n + 1);
        if (!tmp) {
            free(response);
            close(sockfd);
            return NULL;
        }
        response = tmp;
        memcpy(response + response_size, buffer, n);
        response_size += n;
        response[response_size] = '\0';
    }

    close(sockfd);

    return response;
}

// Lua binding: platform.httpGet(url)
static int platform_httpGet(lua_State *L) {
    const char *url = luaL_checkstring(L, 1);
    char *host = NULL;
    char *path = NULL;

    if (parse_url(url, &host, &path) != 0) {
        return luaL_error(L, "Only http:// URLs are supported");
    }

    char *response = http_get(host, path);

    free(host);
    free(path);

    if (!response) {
        return luaL_error(L, "HTTP GET failed");
    }

    // Optional: strip HTTP headers (up to first empty line)
    char *body = strstr(response, "\r\n\r\n");
    if (body) body += 4;
    else body = response;

    lua_pushstring(L, body);
    free(response);
    return 1;
}

// 6. File exists
static int platform_fileExists(lua_State *L) {
    const char *path = luaL_checkstring(L, 1);
    struct stat st;
    lua_pushboolean(L, stat(path, &st) == 0);
    return 1;
}

// 7. List directory contents
static int platform_listDir(lua_State *L) {
    const char *path = luaL_checkstring(L, 1);
    DIR *dir = opendir(path);
    if (!dir) return luaL_error(L, "Could not open directory");

    lua_newtable(L);
    struct dirent *entry;
    int index = 1;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        lua_pushstring(L, entry->d_name);
        lua_rawseti(L, -2, index++);
    }
    closedir(dir);
    return 1;
}

// 8. Create directory
static int platform_createDir(lua_State *L) {
    const char *path = luaL_checkstring(L, 1);
    if (mkdir(path, 0755) != 0) {
        return luaL_error(L, "Failed to create directory");
    }
    lua_pushboolean(L, 1);
    return 1;
}

// 9. Check if path is directory
static int platform_isDir(lua_State *L) {
    const char *path = luaL_checkstring(L, 1);
    struct stat st;
    if (stat(path, &st) != 0) {
        lua_pushboolean(L, 0);
        return 1;
    }
    lua_pushboolean(L, S_ISDIR(st.st_mode));
    return 1;
}

// 10. Check if directory is empty
static int platform_isDirEmpty(lua_State *L) {
    const char *path = luaL_checkstring(L, 1);
    DIR *dir = opendir(path);
    if (!dir) return luaL_error(L, "Could not open directory");

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        // Found a non-dot entry, directory is not empty
        closedir(dir);
        lua_pushboolean(L, 0);
        return 1;
    }
    
    // No non-dot entries found, directory is empty
    closedir(dir);
    lua_pushboolean(L, 1);
    return 1;
}

// Helper function for recursive directory copying
static int copy_directory_recursive(const char *src, const char *dst) {
    DIR *dir = opendir(src);
    if (!dir) return -1;

    // Create destination directory
    if (mkdir(dst, 0755) != 0) {
        closedir(dir);
        return -1;
    }

    struct dirent *entry;
    char src_path[1024], dst_path[1024];
    
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        snprintf(src_path, sizeof(src_path), "%s/%s", src, entry->d_name);
        snprintf(dst_path, sizeof(dst_path), "%s/%s", dst, entry->d_name);

        struct stat st;
        if (stat(src_path, &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            // Recursively copy subdirectory
            if (copy_directory_recursive(src_path, dst_path) != 0) {
                closedir(dir);
                return -1;
            }
        } else {
            // Copy file
            FILE *f1 = fopen(src_path, "rb");
            if (!f1) continue;

            FILE *f2 = fopen(dst_path, "wb");
            if (!f2) {
                fclose(f1);
                continue;
            }

            char buffer[4096];
            size_t bytes;
            while ((bytes = fread(buffer, 1, sizeof(buffer), f1)) > 0) {
                fwrite(buffer, 1, bytes, f2);
            }

            fclose(f1);
            fclose(f2);
        }
    }

    closedir(dir);
    return 0;
}

// 10. Copy directory recursively
static int platform_copyDir(lua_State *L) {
    const char *src = luaL_checkstring(L, 1);
    const char *dst = luaL_checkstring(L, 2);

    struct stat st;
    if (stat(src, &st) != 0 || !S_ISDIR(st.st_mode)) {
        return luaL_error(L, "Source is not a directory or does not exist");
    }

    if (copy_directory_recursive(src, dst) != 0) {
        return luaL_error(L, "Failed to copy directory");
    }

    lua_pushboolean(L, 1);
    return 1;
}

// Register library
static const luaL_Reg platform_lib[] = {
    {"readFile", platform_readFile},
    {"writeFile", platform_writeFile},
    {"copyFile", platform_copyFile},
    {"createJunction", platform_createJunction},
    {"httpGet", platform_httpGet},
    {"fileExists", platform_fileExists},
    {"listDir", platform_listDir},
    {"createDir", platform_createDir},
    {"isDir", platform_isDir},
    {"isDirEmpty", platform_isDirEmpty},
    {"copyDir", platform_copyDir},
    {NULL, NULL}
};

int luaopen_platform(lua_State *L) {
    luaL_newlib(L, platform_lib);
    return 1;
}
