<?php

# example usage: ab -c 100 -n 10000 http://localhost:8080/cache_test.php

header('Cache-Control: public, max-age=800');

$num = 34;
$factorial = 1;
for ($x=$num; $x>=1; $x--)
{
  $factorial = $factorial * $x;
}
echo "Factorial of $num is $factorial\n";

?>
