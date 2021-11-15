PLUGIN = /opt/mmplugins/pcre

FILES  = pcre.env pcre.xc pcre_plugin.so

CFLAGS += -fPIC -g -O2
CFLAGS += -fPIC -Wl,-z,relro
CFLAGS += -Wall -Wextra -Wformat -Werror=format-security -Wdate-time -Werror=implicit-function-declaration -Werror=incompatible-pointer-types -Werror=return-type
CFLAGS += -fno-strict-aliasing -fdebug-prefix-map=$(CURDIR)=.
CFLAGS += -fms-extensions
CFLAGS += -fvisibility=hidden
CFLAGS += $(shell pcre2-config --cflags --libs8)

ifdef TEST
CFLAGS += 
endif

FLAGS_FILE = flags

# name clash with system libpcre.so
pcre_plugin.so: pcre.c Makefile $(FLAGS_FILE)
	gcc -shared -Wl,-soname,$@ -iquote . -o $@ $< $(CFLAGS)

install: $(addprefix $(PLUGIN)/,$(FILES))

$(PLUGIN)/%: % Makefile
	install -o root -g root -m 644 $< $@

$(PLUGIN)/pcre.env: pcre.env Makefile
	install -o root -g root -m 644 $< $@
	sed -i 's,$$PLUGIN,$(PLUGIN),' $@


define NL

endef

CURFLAGS := $(CFLAGS) $(NL)
OLDFLAGS := $(shell cat $(FLAGS_FILE) 2>/dev/null)

$(FLAGS_FILE):
ifneq ($(CURFLAGS),$(OLDFLAGS))
# escape $ORIGIN
	echo "$(subst $$,\$$,$(CURFLAGS))" > $(FLAGS_FILE)
.PHONY: $(FLAGS_FILE)
endif
