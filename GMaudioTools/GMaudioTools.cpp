#define gml extern "C" double

extern "C" double RegisterCallbacks(
    void* async_fn,
    void* ds_map_create,
    void* ds_map_add_double,
    void* ds_map_add_string
) {
    // store these pointers for your cross-platform code…
    return 0;
}
