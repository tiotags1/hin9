
-- php example

-- link the '.php' extension to the php fcgi handler
-- uri is the socket url, atm only accepts tcp sockets
-- max is the max number of workers to spawn, if max is 0 then it will open a new connection for every request
fpm_apps.php = {uri="tcp://localhost:9000", max=4}

-- index directories with the index.php file
table.insert (index_files, 1, "index.php")


-- python example
--fpm_apps.py = {uri="blah"}
--table.insert (index_files, 1, "index.py")


