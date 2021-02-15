
--cflags=""

hin = {
  tag="hin9",
  run_dir=".",
  c_files=list_dir ("src", "*.c"),
  ldflags="-lm -luring -lz -lcrypto -lssl -llua5.1",
  cflags="-D_GNU_SOURCE",
  includes={"src/hin", "src/"},
  req={"basic_pattern", "basic_timer", "basic_hashtable"},
}

if (true) then
  hin.cflags = hin.cflags .. " -O2 -fno-omit-frame-pointer -g"
  hin.ldflags = hin.ldflags .. " -g"
else

end

if (true) then
  table.insert (hin.req, "debug_malloc")
  hin.ldflags = hin.ldflags .. " -ldl"
  hin.cflags = (hin.cflags or "") .. " -DBASIC_USE_MALLOC_DEBUG -include \"debug_malloc.h\""
end

add_lib (hin)

