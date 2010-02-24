require 'mkmf'

#$CFLAGS << ' -fnested-functions'
$CFLAGS << ' -Wall'
$CFLAGS << ' -Werror'

dir_config('fusefs_lib.so')
if have_library('fuse_ino64') || have_library('fuse') 
  create_makefile('fusefs_lib')
else
  puts "No FUSE install available"
end
