
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
