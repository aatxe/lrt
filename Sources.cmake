target_sources(Queijo.Runtime PRIVATE
    runtime/include/queijo/ref.h
    runtime/include/queijo/runtime.h

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
    cli/queijo.cpp
)
