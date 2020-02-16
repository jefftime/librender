srcdir = ./src
libdir ?= ./libs
bindir ?= ./bin
inclibs = render window sized_types error
defs ?=

# Computed
src = $(wildcard $(srcdir)/*.c)
obj = $(addprefix $(bindir)/,$(notdir $(src:%.c=%.o)))
cflags += -fPIC $(addprefix -I,$(incdirs)) $(addprefix -D,$(defs))
incdirs = $(addsuffix /include,$(addprefix $(libdir)/,$(inclibs)))

$(bindir)/librender.a: $(obj)
	$(AR) rcs $@ $(obj)

$(bindir)/%.o: $(srcdir)/%.c
	$(CC) -c $(cflags) -o $@ $^

.PHONY: clean
clean:
	rm -f $(bindir)/librender.a $(obj)
