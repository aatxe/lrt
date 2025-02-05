#!/usr/bin/env python3

import argparse
import os
import subprocess as sp
import sys

from os import path

cwd = os.getcwd()

class ReportableError(Exception):
    """This error object is used to indicate an error should be reported to the user
       without a full stack trace."""
    pass

isMsys = any(sys.platform.startswith(x) for x in ('msys', 'cygwin'))
isWindows = isMsys or sys.platform.startswith('win32')
isMac = sys.platform.startswith('darwin')
isLinux = sys.platform.startswith('linux')
isUnix = not isWindows

targetMap = {
    'lute': {
        'exeName': 'lute',
    },
}

argParser = argparse.ArgumentParser(description='crafting a lute!', formatter_class=argparse.RawDescriptionHelpFormatter)

argParser.add_argument(
    'subcommand', help='command to execute',
    metavar="CMD",
    choices=['configure', 'string', 'build', 'craft', 'run', 'play'],
)

argParser.add_argument(
    'target', help='the thing to build or run',
    metavar="TARGET",
    default="lute",
    choices=targetMap.keys(),
)

argParser.add_argument(
    '-v', '--verbose', dest='verbose', action='store_true',
    help='show verbose compile output'
)

argParser.add_argument(
    '--config', dest='config', action='store',
    choices=['debug', 'release'],
    default='debug',
    help='configuration (default is debug)'
)

argParser.add_argument(
    '--clean', dest='clean', action='store_true',
    help='perform a clean build'
)

argParser.add_argument(
    '--which', '-w', dest='which', action='store_true',
    help='print out the path to the compiled binary and exit'
)

if isWindows:
    vsVersionGroup = argParser.add_mutually_exclusive_group()

    vsVersionGroup.add_argument(
        '--vs2017', dest='vs2017', action='store_true',
        help='Build with vs2017 (default is 2019)'
    )

    vsVersionGroup.add_argument(
        '--vs2022', dest='vs2022', action='store_true',
        help='Build with vs2022 (default is 2019)'
    )

if not isWindows and not isMac and not isLinux:
    raise ReportableError('Unknown platform ' + sys.platform)

argParser.epilog = """
valid subcommands:
  * configure (or string)
  * build (or craft)
  * run (or play)

"""
argParser.epilog += 'valid targets:\n  * ' + '\n  * '.join(sorted(targetMap.keys()))
argParser.epilog += """

note: this script optionally supports the 'argcomplete' module (if installed) to provide
tab completion of command line arguments.  see the documentation for argcomplete for
more setup instructions.
"""

def getSourceRoot():
    cwd = os.getcwd()

    while True:
        if path.isfile(path.join(cwd, '.LUTE_SENTINEL')):
            return cwd

        (newcwd, _) = path.split(cwd)
        if newcwd == cwd:
            break

        cwd = newcwd

    if 'LUTE_ROOT_DIR' in os.environ:
        return os.environ['LUTE_ROOT_DIR']

    raise ReportableError(
        'Could not locate lute project root.  Your cwd must be in your lute checkout, or the LUTE_ROOT_DIR environment variable must be set.')

def cygpath(path):
    if isMsys:
        return sp.check_output(['cygpath', '-u', path]).strip()
    else:
        return path

def winpath(path):
    if isMsys:
        return sp.check_output(['cygpath', '-wa', path]).strip()
    else:
        return path

def getExeName(target):
    baseName = targetMap[target].get('exeName', target)
    ext = '.exe' if isWindows else ''
    return baseName + ext

def getCompiler(args):
    if isMac:
        return 'xcode-15.2'
    elif isWindows and args.vs2017:
        return 'vs2017'
    elif isWindows and args.vs2022:
        return 'vs2022'
    else:
        return 'vs2019'

def getConfig(args):
    return args.config

def getProjectPath(args):
    target = args.target

    buildDir = "build"

    config = getConfig(args).lower()

    return os.path.join(buildDir, getCompiler(args), config)

def projectPathExists(args):
    projectPath = getProjectPath(args)

    return path.isdir(projectPath)

def getExePath(args):
    target = args.target

    exeName = getExeName(target)
    config = getConfig(args)

    buildDir = "build"

    if isWindows or isMac:
        return os.path.join(
            buildDir,
            getCompiler(args),
            config.lower(),
            exeName
        )
    else: # Mac/Linux Ninja
        return os.path.join(
            buildDir,
            config.lower(),
            container,
            exeName
        )

def exeExists(args):
    exePath = getExePath(args)

    return path.isfile(exePath)

def call(*args):
    print('>', ' '.join(map(str, args[0])), file=sys.stderr)
    sys.stderr.flush()

    # If we're running native win32 Python from within an msys bash session,
    # the TEMP environment variable will muck up the compile.  We need to unset it.
    env = os.environ.copy()
    if 'TEMP' in env or 'TMP' in env:
        del env['TEMP']
        del env['TMP']

    if isMsys:
        # We don't want cmake to stumble across msys python.
        env['PATH'] = cygpath('/c/Python27').decode() + os.pathsep + env['PATH']

    return sp.call(*args, env=env)

def build(args):
    targetName = args.target
    projectPath = getProjectPath(args)

    if args.clean:
        call(['ninja', '-C', projectPath, 'clean'])

    cmd = [
        'ninja',
        '-C',
        projectPath,
        targetName
    ]

    return call(cmd)

def getConfigureArguments(args):
    "Returns a list of arguments to pass to configure.py based on the command line arguments provided"
    target = args.target
    projectPath = getProjectPath(args)

    config = getConfig(args).lower()
    if config == "debug":
        config = "Debug"
    elif config == "release":
        config = "Release"

    if args.clean:
        call(['rm', '-rf', projectPath])

    configArgs = [
        '-G=Ninja',
        '-B ' + projectPath,
        '-DCMAKE_BUILD_TYPE=' + config,
        '-DCMAKE_EXPORT_COMPILE_COMMANDS=1',
    ]

    return configArgs

def configure(args):
    """Runs configure.py to generate build files"""
    cmd = [
        "cmake",
    ] + getConfigureArguments(args)

    return call(cmd)

def check(exitCode):
    if exitCode != 0:
        sys.exit(exitCode)

def run(args, unparsed):
    target = args.target

    command = unparsed[:]
    command.insert(0, os.path.abspath(getExePath(args)))

    os.chdir(cwd)

    return call(command)

def main(argv):
    (args, unparsed) = argParser.parse_known_args(argv)
    unparsed = list(u for u in unparsed if u != '--')

    os.chdir(getSourceRoot())

    if args.which:
        print(os.path.abspath(getExePath(args)))
        sys.exit(0)

    subcommand = args.subcommand

    if subcommand == "configure" or subcommand == "string":
        return configure(args)
    elif subcommand == "build" or subcommand == "craft":
        # auto configure if it's not already happened
        if not projectPathExists(args):
            check(configure(args))

        return build(args)
    elif subcommand == "run" or subcommand == "play":
        # auto configure if it's not already happened
        if not projectPathExists(args):
            check(configure(args))

        if not exeExists(args) or args.clean:
            check(build(args))

        return run(args, unparsed)


if __name__ == '__main__':
    # Add tab completion via argcomplete if the module is available
    try:
        import argcomplete
        argcomplete.autocomplete(argParser)
    except:
        pass

    try:
        sys.exit(main(sys.argv[1:]))
    except ReportableError as e:
        print(f'Error: {e}')

