ENDLIST =

#CXXFLAGS = -std=c++11 -Wall -DTRACE_ON -I../include -O0 -g -pg -DNS_THREADED
CXXFLAGS = -std=c++11 -Wall -DTRACE_ON -I../include -O3 -DNDEBUG -DNS_THREADED 

#-DNS_THREADED -- implemented but no significant performance  gain

VPATH = ../include ../lib

#LDFLAGS = -pthread -pg
LDFLAGS = -pthread
LDPATH = -L ../lib
LDLIBS = -lutil

EXECUTABLES = \
	gil \
	columns \
	mat \
	$(ENDLIST)

all: $(EXECUTABLES) git_ignore

COMMON_OBJECTS = \
	$(ENDLIST)

GIL_OBJECTS = \
	gil_main.o \
	Gillespie.o \
	$(ENDLIST)

COLUMNS_OBJECTS = \
	columns.o \
	$(ENDLIST)

MAT_OBJECTS = \
	mat.o \
	$(ENDLIST)

OBJECTS = \
	$(COMMON_OBJECTS) \
	$(GIL_OBJECTS) \
	$(COLUMNS_OBJECTS) \
	$(MAT_OBJECTS) \
	$(ENDLIST)

gil: \
	$(GIL_OBJECTS) $(LDLIBS)
	$(CXX) $(LDFLAGS) $(GIL_OBJECTS) $(LDPATH) $(LDLIBS) -o $@

columns: \
	$(COLUMNS_OBJECTS) $(LDLIBS)
	$(CXX) $(LDFLAGS) $(COLUMNS_OBJECTS) $(LDPATH) $(LDLIBS) -o $@

mat: \
	$(MAT_OBJECTS) $(LDLIBS)
	$(CXX) $(LDFLAGS) $(MAT_OBJECTS) $(LDPATH) $(LDLIBS) -o $@

DEPS = $(subst .o,.d,$(OBJECTS))

clean:
	/bin/rm -f $(DEPS) $(OBJECTS)

veryclean: clean
	/bin/rm -f $(EXECUTABLES) .gitignore

git_ignore:
	@touch .gitignore
	@for e in $(EXECUTABLES); do \
	    if ! egrep "^$$e$$" .gitignore>/dev/null; then \
		echo $$e >> .gitignore; \
	    fi; \
	done

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
