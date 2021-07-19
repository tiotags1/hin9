<html>
<head>
 <title>PHP Post test</title>
</head>
<body>
 <h1>PHP Post test</h1>
 <form action="post.php" method="POST" enctype="multipart/form-data">
  <label for="fname">Name:</label><br />
  <input type="text" id="fname" name="fnam" value="Hello World"/><br />

  <label for="fileToUpload">Select image to upload:</label><br />
  <input type="file" name="fileToUpload" id="fileToUpload"><br />
  <input type="submit" />
 </form>
 <table>
<?php
foreach ($_POST as $key => $value) {
  echo "  <tr><td>$key</td><td>$value</td></tr>\n";
}

foreach ($_FILES as $key => $value) {
  echo "  <tr><td>$key</td><td>${value['name']}</td>";
  echo "  <td>${value['size']}</td>";
  echo "  <td>${value['tmp_name']}</td>";
  echo "  <td>${value['error']}</td>";
  echo "  </tr>\n";
}
?>
 </table>
 </p>Done</p>
</body>
</html>
