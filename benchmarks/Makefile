# Temporary hack that needs to be removed / replaced
-include ../tmp/buildopts.mk
OPTS += $(OPTSATBUILDTIME)

DIRS = \
  ampi \
  charm++ \
  converse \

BGDIRS = \
  ampi \
  charm++ \

FTDIRS = \
  charm++ \

TESTDIRS = $(DIRS)

all: $(foreach i,$(DIRS),build-$i)

test: $(foreach i,$(TESTDIRS),test-$i)

bgtest: $(foreach i,$(filter $(BGDIRS),$(TESTDIRS)),bgtest-$i)

clean: $(foreach i,$(DIRS),clean-$i)
	rm -f TAGS #*#
	rm -f core *~

$(foreach i,$(DIRS),build-$i):
	$(MAKE) -C $(subst build-,,$@) all OPTS='$(OPTS)'

$(foreach i,$(DIRS),test-$i):
	$(MAKE) -C $(subst test-,,$@) test OPTS='$(OPTS)' TESTOPTS='$(TESTOPTS)'

$(foreach i,$(DIRS),bgtest-$i):
	$(MAKE) -C $(subst bgtest-,,$@) bgtest OPTS='$(OPTS)' TESTOPTS='$(TESTOPTS)'

$(foreach i,$(DIRS),clean-$i):
	$(MAKE) -C $(subst clean-,,$@) clean OPTS='$(OPTS)'

test-AMPI: test-ampi
test-charm: test-charm++
