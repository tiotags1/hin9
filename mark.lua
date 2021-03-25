
hin = {
  tag="hin9",
  run_dir=".",
  c_files=list_dir ("src", "*.c"),
  ldflags="-lm -luring -lz -lcrypto -lssl -llua5.1",
  cflags="-D_GNU_SOURCE",
  includes={"src/hin"},
  req={"basic_pattern", "basic_timer", "basic_hashtable", "basic_vfs"},
}

if (true) then
  add_cflags (hin, "-O2 -fno-omit-frame-pointer -g -Wall -Werror=vla")
  add_ldflags (hin, "-g")
end

if (true) then
  add_req (hin, "debug_malloc")
  add_cflags (hin, "-DBASIC_USE_MALLOC_DEBUG -include \"debug_malloc.h\"")
  add_ldflags (hin, "-ldl")
end

add_lib (hin)

