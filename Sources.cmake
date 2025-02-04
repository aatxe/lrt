target_sources(Lute.Runtime PRIVATE
    runtime/include/lute/options.h
    runtime/include/lute/ref.h
    runtime/include/lute/require.h
    runtime/include/lute/runtime.h

    runtime/src/options.cpp
    runtime/src/ref.cpp
    runtime/src/require.cpp
    runtime/src/runtime.cpp
)

target_sources(Lute.Fs PRIVATE
    fs/include/lute/fs.h

    fs/src/fs.cpp
)

target_sources(Lute.Luau PRIVATE
    luau/include/lute/luau.h

    luau/src/luau.cpp
)

target_sources(Lute.Net PRIVATE
    net/include/lute/net.h

    net/src/net.cpp
)

target_sources(Lute.Task PRIVATE
    task/include/lute/task.h

    task/src/task.cpp
)

target_sources(Lute.VM PRIVATE
    vm/include/lute/spawn.h
    vm/include/lute/vm.h

    vm/src/spawn.cpp
    vm/src/vm.cpp
)

target_sources(Lute.CLI PRIVATE
    cli/main.cpp
    cli/tc.h
    cli/tc.cpp
)
