--TEST--
Check for correct realpath'ing of incpath
--SKIPIF--
<?php if (!extension_loaded("incpath")) print "skip"; ?>
--FILE--
<?php 
echo "incpath extension is available\n";

$real_include_path = "/tmp/mod_ip.true";
$linked_include_path = "/tmp/mod_ip.softlink";
exec("rm -rf $real_include_path");
exec("rm -rf $linked_include_path");
$cwd = getcwd();

$variations = array();
$variations[] = array(
  'original_include_path' => $linked_include_path,
  'replacement_pattern' => $linked_include_path,
  'verification_include_path' => $real_include_path,
);
$variations[] = array(
  'original_include_path' => $linked_include_path . ':/tmp',
  'replacement_pattern' => $linked_include_path,
  'verification_include_path' => $real_include_path . ':/tmp',
);
$variations[] = array(
  'original_include_path' => '/tmp:' . $linked_include_path,
  'replacement_pattern' => $linked_include_path,
  'verification_include_path' => '/tmp:' . $real_include_path,
);
$variations[] = array(
  'original_include_path' => '/bin:' . $linked_include_path . ':/tmp',
  'replacement_pattern' => $linked_include_path,
  'verification_include_path' => '/bin:' . $real_include_path . ':/tmp',
);
$variations[] = array(
  'original_include_path' => $linked_include_path . ':.:/tmp',
  'replacement_pattern' => $linked_include_path . ':.',
  'verification_include_path' => $real_include_path . ":$cwd:" . '/tmp',
);
$variations[] = array(
  'original_include_path' => '/bin:' . $linked_include_path . ':.:/tmp',
  'replacement_pattern' => $linked_include_path . ':.',
  'verification_include_path' => '/bin:' . $real_include_path . ":$cwd" . ':/tmp',
);
$variations[] = array(
  'original_include_path' => '/bin:' . $linked_include_path . ':.',
  'replacement_pattern' => $linked_include_path . ':.',
  'verification_include_path' => '/bin:' . $real_include_path . ":$cwd",
);
$variations[] = array(
  'original_include_path' => '/bin:.',
  'replacement_pattern' => $linked_include_path . ':.',
  'verification_include_path' => '/bin:.',
);

if (mkdir($real_include_path, 0777, true) !== true) {
  echo "failed to mkdir $real_include_path\n";
  die();
}
if (symlink($real_include_path, $linked_include_path) !== true) {
  echo "failed to symlink $real_include_path to $linked_include_path\n";
  die();
}

// The following command sets include_path to $linked_include_path using -d, but expects
// the output of the echo to be $real_include_path

foreach ($variations as $v) {
  $command = "php -d \"incpath.realpath_sapi_list=cli\" -d \"include_path={$v['original_include_path']}\"";
  $command .= " -d \"incpath.search_replace_pattern={$v['replacement_pattern']}\"";
  $command .= " -r \"echo get_include_path();\"";

  $output = exec($command);
  if ($output === false) {
    echo "Failed to execute $command\n";
    die();
  }

  $output = trim($output);

  if ($output != $v['verification_include_path']) {
    echo "$output does not match $real_include_path\n";
    die();
  }
}

echo "Successfully verified\n";

?>
--CLEAN--
<?php
$real_include_path = "/tmp/mod_ip.true";
$linked_include_path = "/tmp/mod_ip.softlink";
exec("rm -rf $real_include_path");
exec("rm -rf $linked_include_path");
?>
--EXPECT--
incpath extension is available
Successfully verified
