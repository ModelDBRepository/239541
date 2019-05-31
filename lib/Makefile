ENDLIST =

#CXXFLAGS = -std=c++11 -Wall -DTRACE_ON -I../include -O0  -g -pg -DUTIL_THREADED 
CXXFLAGS = -std=c++11 -Wall -DTRACE_ON -I../include -O3 -DNDEBUG

CFLAGS = -I../include -O3 -DNDEBUG
ARFLAGS = rvU

LIBUTIL = libutil.a
LIBS = $(LIBUTIL)

LIBUTIL_OBJECTS = \
	$(LIBUTIL)(format.o) \
	$(LIBUTIL)(tinyexpr.o) \
	$(LIBUTIL)(Sched.o) \
	$(LIBUTIL)(Trace.o) \
	$(LIBUTIL)(Util.o) \
	$(ENDLIST)

UNUSED_OBJECTS = \
	$(LIBUTIL)(Props.o) \
	$(LIBUTIL)(PthreadPool.o) \
	$(ENDLIST)

LIBOBJECTS = \
	$(LIBUTIL_OBJECTS) \
	$(ENDLIST)

all: $(LIBOBJECTS)

clean:
	/bin/rm -f $(LIBS)

veryclean: clean

# Automated dependency management

%.o: %.cc
	$(CXX) -c $(CXXFLAGS) $*.cc -o $*.o
	$(CXX) -MM $(CXXFLAGS) $*.cc > $*.d
	@mv -f $*.d $*.d.tmp
	@sed -e 's|.*:|$*.o:|' < $*.d.tmp > $*.d
	@sed -e 's/.*://' -e 's/\\$$//' < $*.d.tmp | fmt -1 | \
	  sed -e 's/^ *//' -e 's/$$/:/' >> $*.d
	@rm -f $*.d.tmp

-include $(OBJECTS:.o=.d)
