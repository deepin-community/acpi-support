all: acpi_fakekey acpi_fakekeyd

acpi_fakekey: acpi_fakekey.c
	gcc -Wall $(shell dpkg-buildflags --get CFLAGS) $(shell dpkg-buildflags --get CPPFLAGS) $(shell dpkg-buildflags --get LDFLAGS) -o $@ $<

acpi_fakekeyd: acpi_fakekeyd.c
	gcc -Wall $(shell dpkg-buildflags --get CFLAGS) $(shell dpkg-buildflags --get CPPFLAGS) $(shell dpkg-buildflags --get LDFLAGS) $(shell pkg-config --cflags libsystemd) -o $@ $< $(shell pkg-config --libs libsystemd)

clean:
	$(RM) acpi_fakekey acpi_fakekeyd
