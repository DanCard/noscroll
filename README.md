# noscroll
Helpful when doing something that outputs large amount of information too fast to read.
This will stop the terminal from scrolling which makes some of the output easier to read.
Instead of scrolling output is top to bottom and back to top again.
Log file is automatically created, file name is command to be executed.

Usage:
ns -c "<command to execute>"
ns -c "echo -en 'bob\ndave\njoe\n'"
ns -c "make -j49"
