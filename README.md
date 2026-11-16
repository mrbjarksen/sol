# Sol sunlight alarm over HDMI-CEC

Sol is a sunlight alarm that uses HDMI-CEC to wake the TV, and creates a full-screen X11 window
that slowly changes colors to simulate sunrise at dawn, then turns the TV off again.

It is most convenient to use on a Raspberry Pi connected to a TV with an X window server running.

## Run

**Fetch:**
```
$ git clone https://github.com/mrbjarksen/sol
```

**Configure:**
See Makefile for valid options.
```
make configure [desired overrides]
```

**Build:**
Ensure libcec and libx11 are installed.
```
$ make
```

**Install:**
Both binary and systemd timer/service are installed.
Default alarm starts at 06:30 on weekdays and lasts for 30 minutes.
```
$ make install
```

**Run:**
```
$ sol [duration in minutes]
```

**Uninstall:**
```
$ make uninstall
```
