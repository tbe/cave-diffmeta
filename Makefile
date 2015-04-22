CPPFLAGS=-std=c++11 $(shell pkg-config --cflags paludis)
LDFLAGS=-std=c++11 $(shell pkg-config --libs paludis)

SRCS=example_command_line.cc cave-metadiff.cc
OBJS=$(subst .cc,.o,$(SRCS))

all: cave-metadiff

cave-metadiff: $(OBJS)
	$(CXX) $(LDFLAGS) -o cave-metadiff $(OBJS)

depend: .depend

.depend: $(SRCS)
	rm -f ./.depend
	$(CXX) $(CPPFLAGS) -MM $^>>./.depend;

clean:
	$(RM) $(OBJS)

dist-clean: clean
	$(RM) *~ .depend

include .depend