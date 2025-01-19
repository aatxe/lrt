#include "queijo/options.h"

// TODO: this is never set to true today
static bool codegen = false;

Luau::CompileOptions copts()
{
    Luau::CompileOptions result = {};
    result.optimizationLevel = 2;
    result.debugLevel = 2;
    result.typeInfoLevel = 1;
    result.coverageLevel = 0;

    return result;
}

bool getCodegenEnabled()
{
    return codegen;
}
