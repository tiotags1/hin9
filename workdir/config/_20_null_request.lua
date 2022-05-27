

map (main, "/helloworld", 0, function (req)
  respond (req, 200, "Hello world\n")
  return true
end)

