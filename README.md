# libipho-screen-server

This application forwards image data and other commands
from the libipho embedded board to an Android app.
The Android app constitutes the main screen of the
libipho photobooth that shows the most recently taken image.

The application is written in C for Linux. It receives commands from
the libipho-core scripts using a named pipe and forwards image data
and commands to an Android app using a TCP socket connection.
