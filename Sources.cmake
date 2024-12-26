target_sources(Queijo.Runtime PRIVATE
    runtime/include/queijo/ref.h
    runtime/include/queijo/runtime.h

    runtime/src/ref.cpp
    runtime/src/runtime.cpp
)

target_sources(Queijo.Fs PRIVATE
    fs/include/queijo/fs.h

    fs/src/fs.cpp
)

target_sources(Queijo.Luau PRIVATE
    luau/include/queijo/luau.h

    luau/src/luau.cpp
)

target_sources(Queijo.Net PRIVATE
    net/include/queijo/net.h

    net/src/net.cpp
)

target_sources(Queijo.Task PRIVATE
    task/include/queijo/task.h

    task/src/task.cpp
)

target_sources(Queijo.CLI PRIVATE
    cli/options.h
    cli/options.cpp
    cli/main.cpp
    cli/require.h
    cli/require.cpp
    cli/spawn.h
    cli/spawn.cpp
    cli/tc.h
    cli/tc.cpp
)
