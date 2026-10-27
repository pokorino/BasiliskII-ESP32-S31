# Upstream and attribution

This experimental S31 port was adapted from the local ESP32-P4 port based on:

- [amcchord/M5Tab-Macintosh](https://github.com/amcchord/M5Tab-Macintosh),
  the ESP32-P4 BasiliskII port. The exact upstream source commit was not recorded
  in the local source snapshot.
- [BasiliskII / macemu](https://github.com/cebix/macemu), originally by
  Christian Bauer and contributors.
- UAE CPU emulation, including Bernd Schmidt and contributors, as identified
  in the source file notices.

The original per-file copyright and license notices remain in the source tree.
The GPL version 2 text in LICENSE is taken from the BasiliskII COPYING file at:
https://github.com/cebix/macemu/blob/master/BasiliskII/COPYING
Files specifying GPL version 2 or later retain that choice; other per-file
notices remain applicable.

Dependencies are fetched by ESP-IDF Component Manager, not vendored in this
public source package. Their own copyright and license notices apply.
