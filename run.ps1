param (
  [switch]$crt
)
$cargs = @("-march=native", "-masm=intel", "-std=gnu99", "-fno-signed-char")
if ($crt) {
  $cargs += @("-DHAS_CRT", "-DQFLOAT_HAS_CRT")
} else {
  $cargs += @("-nostdlib", "-mno-stack-arg-probe")
}
$cargs += @("-Werror", "-Wconversion", "-Wsign-conversion", "-Wnullable-to-nonnull-conversion")
$cargs += @("-fuse-ld=lld", "-Wl,/STACK:0x100000")
if ($opt) {
  $cargs += @("-O2", "-flto", "-g")
} else {
  $cargs += @("-O0", "-g")
}
$input = "test/test_fmt_float.c"
$output = "test_fmt_float"

rm ($output + ".rdi") -ErrorAction SilentlyContinue;
rm ($output + ".pdb") -ErrorAction SilentlyContinue;
echo "clang $cargs $input -o $output.exe && $output.exe"
clang $cargs $input -o "$output.exe" && Invoke-Expression "./$output.exe"
