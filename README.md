# File-System

Commands: 
quit,
df,
list,
put,
get,
del,
undel,
createfs,
open,
savefs,
attrib


Execute the file system using command prompt 

Test run example (commands):

1) Start the program 
2) createfs fs.image
3) open fs.image
4) put testfile
5) savefs fs.image
6) close 
7) quit
8) Restart the program 
9) open fs.image
10) get testfile testfile.new
11) close
12) quit

Then run:

diff testfile testfile.new

Nothing should return which means the files match
