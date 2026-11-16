CC      ?= cc
CFLAGS  ?= -O2 -Wall -Wextra -std=c11
LDLIBS  ?= -lcec -lX11 -lm

PREFIX  ?= /usr/local
BINDIR  ?= $(PREFIX)/bin
UNITDIR ?= /etc/systemd/system

USER       ?= pi
XAUTHORITY ?= /home/$(USER)/.Xauthority

ALARMTIME   ?= Mon..Fri 06:30
ALARMDURMIN ?= 30

-include config.mk

TARGET := sol
SRC    := sol.c

SERVICE := sol.service
TIMER   := sol.timer

.PHONY: all configure install uninstall clean

all: $(TARGET) $(SERVICE) $(TIMER)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LDLIBS)

$(SERVICE): sol.service.in
	sed 's|@ExecStart@|$(BINDIR)/$(TARGET) $(ALARMDURMIN)|g; s|@User@|$(USER)|g; s|@XAuthority@|$(XAUTHORITY)|g' $< > $@

$(TIMER): sol.timer.in
	sed 's|@Unit@|$(SERVICE)|g; s|@OnCalendar@|$(ALARMTIME)|g' $< > $@

configure:
	@echo "PREFIX = $(PREFIX)" > config.mk
	@echo "BINDIR = $(BINDIR)" >> config.mk
	@echo "CC = $(CC)" >> config.mk
	@echo "CFLAGS = $(CFLAGS)" >> config.mk
	@echo "LDLIBS = $(LDLIBS)" >> config.mk
	@echo "Configuration saved to config.mk"

install: all
	install -Dm755 $(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)
	install -Dm644 $(SERVICE) $(DESTDIR)$(UNITDIR)/$(SERVICE)
	install -Dm644 $(TIMER) $(DESTDIR)$(UNITDIR)/$(TIMER)
	@if [ -z "$(DESTDIR)" ]; then \
		systemctl daemon-reload; \
		systemctl enable $(TIMER); \
	fi

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(TARGET)
	rm -f $(DESTDIR)$(UNITDIR)/$(SERVICE)
	rm -f $(DESTDIR)$(UNITDIR)/$(TIMER)
	@if [ -z "$(DESTDIR)" ]; then \
		systemctl daemon-reload; \
	fi

clean:
	rm -f $(TARGET) $(SERVICE) $(TIMER) config.mk

.PHONY: clean
