#include "tc.h"

#include "Luau/BuiltinDefinitions.h"
#include "Luau/Error.h"
#include "Luau/Transpiler.h"
#include "Luau/TypeAttach.h"

static const std::string kQueijoDefinitions = R"QUEIJO_TYPES(
-- Net api
declare net: {
    get: (string) -> string,
    getAsync: (string) -> string,
}
-- fs api
declare class file end
declare fs: {
 -- probably not the correct sig
    open: (string, "r" | "w" | "a" | "r+" | "w+") -> file,
    close: (file) -> (),
    read: (file) -> string,
    write: (file, string) -> (),
    readfiletostring : (string) -> string,
    writestringtofile : (string, string) -> (),
 -- is this right? I feel like we want a promise type here
    readasync : (string) -> string,
}

-- globals
declare function spawn(path: string): any

)QUEIJO_TYPES";

struct QueijoFileResolver : Luau::FileResolver
{
    std::optional<Luau::SourceCode> readSource(const Luau::ModuleName& name) override
    {
        Luau::SourceCode::Type sourceType;
        std::optional<std::string> source = std::nullopt;

        // If the module name is "-", then read source from stdin
        if (name == "-")
        {
            source = readStdin();
            sourceType = Luau::SourceCode::Script;
        }
        else
        {
            source = readFile(name);
            sourceType = Luau::SourceCode::Module;
        }

        if (!source)
            return std::nullopt;

        return Luau::SourceCode{*source, sourceType};
    }

    std::optional<Luau::ModuleInfo> resolveModule(const Luau::ModuleInfo* context, Luau::AstExpr* node) override
    {
	// TODO: Need to handle requires
        return std::nullopt;
    }

    std::string getHumanReadableModuleName(const Luau::ModuleName& name) const override
    {
        if (name == "-")
            return "stdin";
        return name;
    }

private:
// TODO: add require resolver;
};

struct QueijoConfigResolver : Luau::ConfigResolver
{
    Luau::Config defaultConfig;

    mutable std::unordered_map<std::string, Luau::Config> configCache;
    mutable std::vector<std::pair<std::string, std::string>> configErrors;

    QueijoConfigResolver(Luau::Mode mode)
    {
        defaultConfig.mode = mode;
    }

    const Luau::Config& getConfig(const Luau::ModuleName& name) const override
    {
        std::optional<std::string> path = getParentPath(name);
        if (!path)
            return defaultConfig;

        return readConfigRec(*path);
    }

    const Luau::Config& readConfigRec(const std::string& path) const
    {
        auto it = configCache.find(path);
        if (it != configCache.end())
            return it->second;

        std::optional<std::string> parent = getParentPath(path);
        Luau::Config result = parent ? readConfigRec(*parent) : defaultConfig;

        std::string configPath = joinPaths(path, Luau::kConfigName);

        if (std::optional<std::string> contents = readFile(configPath))
        {
            Luau::ConfigOptions::AliasOptions aliasOpts;
            aliasOpts.configLocation = configPath;
            aliasOpts.overwriteAliases = true;

            Luau::ConfigOptions opts;
            opts.aliasOptions = std::move(aliasOpts);

            std::optional<std::string> error = Luau::parseConfig(*contents, result, opts);
            if (error)
                configErrors.push_back({configPath, *error});
        }

        return configCache[path] = result;
    }
};

static void report(const char* name, const Luau::Location& loc, const char* type, const char* message)
{
    // fprintf(stderr, "%s(%d,%d): %s: %s\n", name, loc.begin.line + 1, loc.begin.column + 1, type, message);
    int columnEnd = (loc.begin.line == loc.end.line) ? loc.end.column : 100;

    // Use stdout to match luacheck behavior
    fprintf(stdout, "%s:%d:%d-%d: (W0) %s: %s\n", name, loc.begin.line + 1, loc.begin.column + 1, columnEnd, type, message);
}

static void reportError(const Luau::Frontend& frontend, const Luau::TypeError& error)
{
    std::string humanReadableName = frontend.fileResolver->getHumanReadableModuleName(error.moduleName);

    if (const Luau::SyntaxError* syntaxError = Luau::get_if<Luau::SyntaxError>(&error.data))
        report(humanReadableName.c_str(), error.location, "SyntaxError", syntaxError->message.c_str());
    else
        report(
            humanReadableName.c_str(),
            error.location,
            "TypeError",
            Luau::toString(error, Luau::TypeErrorToStringOptions{frontend.fileResolver}).c_str()
        );
}

static void reportWarning(const char* name, const Luau::LintWarning& warning)
{
    report(name, warning.location, Luau::LintWarning::getName(warning.code), warning.text.c_str());
}

static bool reportModuleResult(Luau::Frontend& frontend, const Luau::ModuleName& name, bool annotate)
{
    std::optional<Luau::CheckResult> cr = frontend.getCheckResult(name, false);

    if (!cr)
    {
        fprintf(stderr, "Failed to find result for %s\n", name.c_str());
        return false;
    }

    if (!frontend.getSourceModule(name))
    {
        fprintf(stderr, "Error opening %s\n", name.c_str());
        return false;
    }

    for (auto& error : cr->errors)
        reportError(frontend,  error);

    std::string humanReadableName = frontend.fileResolver->getHumanReadableModuleName(name);
    for (auto& error : cr->lintResult.errors)
        reportWarning( humanReadableName.c_str(), error);
    for (auto& warning : cr->lintResult.warnings)
        reportWarning( humanReadableName.c_str(), warning);

    return cr->errors.empty() && cr->lintResult.errors.empty();
}


int typecheck(const std::vector<std::string> sourceFiles)
{
    Luau::Mode mode = Luau::Mode::Strict;
    bool annotate = true;
    std::string basePath = "";

    Luau::FrontendOptions frontendOptions;
    frontendOptions.retainFullTypeGraphs = annotate;
    frontendOptions.runLintChecks = true;

    QueijoFileResolver fileResolver;
    QueijoConfigResolver configResolver(mode);
    Luau::Frontend frontend(&fileResolver, &configResolver, frontendOptions);

    Luau::registerBuiltinGlobals(frontend, frontend.globals);
    Luau::LoadDefinitionFileResult loadResult =
        frontend.loadDefinitionFile(frontend.globals, frontend.globals.globalScope, kQueijoDefinitions, "@luau", false, false);
    LUAU_ASSERT(loadResult.success);
    Luau::freeze(frontend.globals.globalTypes);

    for (const std::string& path : sourceFiles)
        frontend.queueModuleCheck(path);

    std::vector<Luau::ModuleName> checkedModules;
    try
    {
        checkedModules = frontend.checkQueuedModules(std::nullopt);
    }
    catch (const Luau::InternalCompilerError& ice)
    {
        Luau::Location location = ice.location ? *ice.location : Luau::Location();

        std::string moduleName = ice.moduleName ? *ice.moduleName : "<unknown module>";
        std::string humanReadableName = frontend.fileResolver->getHumanReadableModuleName(moduleName);

        Luau::TypeError error(location, moduleName, Luau::InternalError{ice.message});
               report(
            humanReadableName.c_str(),
            location,
            "InternalCompilerError",
            Luau::toString(error, Luau::TypeErrorToStringOptions{frontend.fileResolver}).c_str()
        );
        return 1;
    }

    int failed = 0;

    for (const Luau::ModuleName& name : checkedModules)
        failed += !reportModuleResult(frontend, name,  annotate);

    if (!configResolver.configErrors.empty())
    {
        failed += int(configResolver.configErrors.size());

        for (const auto& pair : configResolver.configErrors)
            fprintf(stderr, "%s: %s\n", pair.first.c_str(), pair.second.c_str());
    }

    return failed ? 1 : 0;
}
