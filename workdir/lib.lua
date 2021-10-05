
function printf (...)
  io.write (string.format (...))
end

function sec_to_str (sec)
  local pre = ""
  if (sec == nil) then return "nil" end
  if (sec < 0) then
    sec = -sec
    pre = "-"
  end
  if (sec < 60) then
    return string.format ("%s%.0f second(s)", pre, sec)
  end
  sec = sec / 60
  if (sec < 60) then
    return string.format ("%s%.0f minute(s)", pre, sec)
  end
  sec = sec / 60
  if (sec < 24) then
    return string.format ("%s%.0f hour(s)", pre, sec)
  end
  sec = sec / 24
  if (sec < 365) then
    return string.format ("%s%.0f day(s)", pre, sec)
  end
  sec = sec / 366
  return string.format ("%s%.0f year(s)", pre, sec)
end

local age = file_age ("workdir/ssl/cert.pem")

local renew_time = nil

function init ()
  if (auto_ssl_renew) then
    renew_time = (auto_ssl_renew or 0) - (age or 0)
    printf ("cert age is %s due in %s\n", sec_to_str (age), sec_to_str (renew_time))
    if (ssl_sock == nil) then
      auto_ssl_do_renew ()
    end
  end
end

function auto_ssl_time_callback (dt)
  renew_time = renew_time - dt
  if (renew_time < 0) then
    renew_time = (renew_time + auto_ssl_renew)
    auto_ssl_do_renew ()
  end
end

function auto_ssl_do_renew ()
  if (auto_ssl_renew and auto_ssl_script) then
    exec {path=auto_ssl_script, mode="fork", callback=function (ret, err)
    end}
  end
end

function redirect (req, uri, status)
  if (status == nil) then status = 302 end
  add_header (req, "Location", uri)
  return respond (req, status, nil)
end

function require_dir (path)
  local dir, err = list_dir1 (path)
  if (dir == nil or err) then
    printf ("require dir %s gave error %s\n", path, err)
    return
  end
  table.sort (dir, function(a, b) return a:upper() < b:upper() end)
  for i, fname in pairs (dir) do
    if (not string.match (fname, '^_')
      and string.match (fname, '.lua$')) then
      --printf ("found config file to include '%s / %s'\n", path, fname)
      local new = string.format ("%s/%s", path, fname)
      local err = include (new)
      if (err) then
        printf ("%s: %s\n", new, err)
        error ()
      end
    end
  end
end

function create_round_robin (...)
  local arg = {...}
  if (#arg < 1) then return nil end
  local n = 1
  return function (req, url)
    local value = arg[n]
    n = n + 1
    if (n > #arg) then n = 1 end
    return proxy (req, value .. url)
  end
end


