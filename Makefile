# LibrePadPS5 build and host verification entry points.
.DEFAULT_GOAL := all
PS5_HOST ?=
PS5_PORT ?= 9021

.PHONY: all payload xbox xbox-module send check test host-test clean help

all: payload

payload:
ifndef PS5_PAYLOAD_SDK
	$(error Set PS5_PAYLOAD_SDK=/path/to/ps5-payload-sdk)
endif
	$(MAKE) -f config/build/payload.mk

xbox:
	$(MAKE) -f config/build/xbox.mk

xbox-module:
	$(MAKE) -f config/build/xbox.mk xbox-module

send: payload
ifeq ($(strip $(PS5_HOST)),)
	$(error Set PS5_HOST to your console's address)
endif
	python3 -c "import socket; f=open('dist/LibrePadPS5.elf','rb').read(); s=socket.create_connection(('$(PS5_HOST)', $(PS5_PORT)), timeout=5); s.sendall(f); s.close(); print('Payload sent')"

check:
	python3 tools/check_style.py
	python3 tools/check_repository.py

test: payload-test bluetooth-test xbox-test
host-test: test

clean:
	rm -rf build

help:
	@echo "make                 Build the main payload (requires PS5_PAYLOAD_SDK)"
	@echo "make WITH_XBOX=0     Build USB/Bluetooth without Xbox Wireless"
	@echo "make xbox            Build the standalone Xbox diagnostic payload"
	@echo "make xbox-module     Build the Xbox static archive"
	@echo "make test            Run all C/C++ host regression suites"
	@echo "make dashboard-test  Run Vue dashboard checks (run npm --prefix web ci first)"
	@echo "make send PS5_HOST=...  Build and send to your console"

include config/build/tests.mk
