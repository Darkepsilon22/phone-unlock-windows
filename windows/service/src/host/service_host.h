#pragma once

#include <windows.h>

void RunService(HANDLE stopEvent);

int RunAsService();

int RunInConsole();
