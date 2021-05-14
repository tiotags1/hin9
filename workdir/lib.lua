
function printf (...)
  io.write (string.format (...))
end

local renew_time = nil

function auto_ssl_time_callback (dt)
  renew_time = (renew_time or auto_ssl_renew) - dt
  if (renew_time < 0) then
    renew_time = (renew_time + auto_ssl_renew)
    auto_ssl_do_renew ()
  end
end

function auto_ssl_do_renew ()
  if (auto_ssl_script) then
    exec {path=auto_ssl_script, mode="fork", callback=function (ret, err)
    end}
  end
end
