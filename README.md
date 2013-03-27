Cinder-ClientServer
===================

A TCP Server/Client for Cinder - Currently a very minimal implementation used for an internal project.

### Winsock error ###
If you get the following error:

> WinSock.h has already been included

This might be caused because you already included some boost libraries before this CinderBlock. Known workarounds (but not always effective) are adding

> \#define WIN32_LEAN_AND_MEAN

or

> \#include <winsock2.h>

in the your main app file.

But simpler is to make sure that TCP.h gets included sooner.

> \#include "TCP.h"
