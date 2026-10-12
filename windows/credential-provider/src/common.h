#pragma once

#include <windows.h>
#include <ntsecapi.h>
#include <credentialprovider.h>
#include <wincred.h>
#include <shlwapi.h>
#include <shlguid.h>
#include <propkey.h>
#include <strsafe.h>
#include <new>

#include "guid.h"
#include "log.h"

void DllAddRef();
void DllRelease();
