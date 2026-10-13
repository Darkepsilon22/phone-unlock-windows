#include "admin_commands.h"
#include "service_host.h"
#include <fcntl.h>
#include <io.h>
#include <cstdio>

int wmain(int argc, wchar_t** argv)
{
    if (argc < 2)
    {
        return RunAsService();
    }
    _setmode(_fileno(stdout), _O_U16TEXT);
    _setmode(_fileno(stderr), _O_U16TEXT);
    return RunAdminCommand(argc - 1, argv + 1);
}
