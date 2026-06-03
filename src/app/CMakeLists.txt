add_executable(gateway main.c)
add_executable(simulator
    simulator.c
    data_gen.c
    pipe_util.c
    fifo_writer.c
)

target_compile_definitions(simulator PRIVATE _POSIX_C_SOURCE=199309L)

install(TARGETS gateway RUNTIME COMPONENT gateway)
install(TARGETS simulator RUNTIME COMPONENT simulator)
