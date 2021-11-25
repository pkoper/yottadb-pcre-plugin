ifeq (,$(ydb_dist))
  $(error $$ydb_dist not defined)
endif

PCREFLAGS ?= $(shell pcre2-config --cflags --libs8)
ifeq (,$(PCREFLAGS))
  $(error pcre2-config is missing)
endif

PLUGIN = $(ydb_dist)/plugin

FILES  = pcre.env pcre.xc pcre_plugin.so
MFILES = pcreexamples.m

CFLAGS += -fPIC -g -O2
CFLAGS += -Wl,-z,relro
CFLAGS += -Wall -Wextra -Wformat -Werror=format-security -Wdate-time -Werror=implicit-function-declaration -Werror=incompatible-pointer-types -Werror=return-type
CFLAGS += -fno-strict-aliasing -fdebug-prefix-map=$(CURDIR)=.
CFLAGS += -fms-extensions
CFLAGS += -fvisibility=hidden
CFLAGS += $(PCREFLAGS)

ifdef COVER
# TODO
CFLAGS += -DCOVER
endif

FLAGS_FILE = flags

# Plugin name pcre.so will cause a name clash with system libpcre.so, using pcre_plugin.so instead
pcre_plugin.so: pcre.c Makefile $(FLAGS_FILE)
	gcc -shared -Wl,-soname,$@ -iquote . -o $@ $< $(CFLAGS)

install:: $(addprefix $(PLUGIN)/,$(FILES))
install:: $(addprefix $(PLUGIN)/r/,$(MFILES))

$(PLUGIN)/%: % Makefile
	install -o root -g root -m 644 $< $@

$(PLUGIN)/pcre.env: pcre.env Makefile
	install -o root -g root -m 644 $< $@
	sed -i 's,$$PLUGIN,$(PLUGIN),' $@

$(PLUGIN)/r/pcreexamples.m: pcreexamples.m Makefile
	install -o root -g root -m 644 $< $@

clean:
	rm -f pcre_plugin.so $(FLAGS_FILE)

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
