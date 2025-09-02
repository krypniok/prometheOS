#pragma once

#include "tsqlfs.h"

// Standard-like aliases to our DB-backed file API
typedef TSQLFILE FILE;

#define fopen  tsql_fopen
#define fread  tsql_fread
#define fwrite tsql_fwrite
#define fclose tsql_fclose

