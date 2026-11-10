.PHONY: build test check

build:
	python3 -m compileall -q gnuchan_bluetooth

test:
	python3 -m unittest discover -s tests -v

check:
	python3 -m compileall -q tests
	sh -n scripts/install.sh scripts/uninstall.sh
