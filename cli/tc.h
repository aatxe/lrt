#pragma once

#include "Luau/FileResolver.h"
#include "Luau/Frontend.h"
#include "Luau/FileUtils.h"

int typecheck(const std::vector<std::string>& sourceFiles);
