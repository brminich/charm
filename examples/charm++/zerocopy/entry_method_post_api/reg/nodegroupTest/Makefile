-include ../../../../../common.mk
-include ../../../../../../include/conv-mach-opt.mak
CHARMC=../../../../../../bin/charmc $(OPTS)

all: nodegroupTest

nodegroupTest:  nodegroupTest.o
	$(CHARMC) nodegroupTest.o -o nodegroupTest -language charm++ -module CommonLBs

cifiles: nodegroupTest.ci
	$(CHARMC) -c nodegroupTest.ci
	touch cifiles

nodegroupTest.o : nodegroupTest.C cifiles
	$(CHARMC) -c nodegroupTest.C

test: all
ifeq ($(CMK_MULTICORE),1)
	echo "Skipping nodegroupTest test on MULTICORE builds as it has to be run on even number of logical nodes"
else
	$(call run, +p4 ./nodegroupTest)
endif

clean:
	rm -f *.def.h *.decl.h *.o *~ *.exe cifiles charmrun nodegroupTest
