#ifndef _IOUTILS_H_
#define _IOUTILS_H_

#include <fstream>

static char *io_read_bytecode(const char *path, size_t *size)
{
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open())
        throw std::runtime_error("error open file failed!");

    *size = file.tellg();
    file.seekg(0);

    /* malloc buffer */
    char *buf = (char *) malloc(*size);
    file.read(buf, *size);
    file.close();

    return buf;
}

static void io_free_buf(char *buf)
{
    free(buf);
}

#endif /* _IOUTILS_H_ */
