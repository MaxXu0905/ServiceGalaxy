TEST_LIBDIR=../lib
TEST_HEADERDIR=../include

GTEST_DIR = .
CPPFLAGS += -I$(GTEST_DIR)/include
LDFLAGS= -lpthread
CXXFLAGS += -g -Wall -fPIC -Wextra -L.

LIBS = libgtest.so libgtest_main.a
TARGET = $(LIBS)

GTEST_HEADERS = $(GTEST_DIR)/include/gtest/*.h \
                $(GTEST_DIR)/include/gtest/internal/*.h

all : $(LIBS) install
GTEST_SRCS_ = $(GTEST_DIR)/src/*.cc $(GTEST_DIR)/src/*.h $(GTEST_HEADERS)

gtest-all.o : $(GTEST_SRCS_)
	$(CXX) $(CPPFLAGS) -I$(GTEST_DIR) $(CXXFLAGS) -c \
            $(GTEST_DIR)/src/gtest-all.cc

gtest_main.o : $(GTEST_SRCS_)
	$(CXX) $(CPPFLAGS) -I$(GTEST_DIR) $(CXXFLAGS) -c \
            $(GTEST_DIR)/src/gtest_main.cc
HtmlPrinter.o:$(GTEST_SRCS_) $(GTEST_DIR)/src/HtmlPrinter.cpp
	$(CXX) $(CPPFLAGS) -I$(GTEST_DIR) $(CXXFLAGS) -c \
            $(GTEST_DIR)/src/HtmlPrinter.cpp
libgtest.so :gtest-all.o HtmlPrinter.o
	$(CC) $(CXXFLAGS) $(LDFLAGS) gtest-all.o HtmlPrinter.o -shared -o$@

libgtest_main.a : gtest_main.o gtest-all.o HtmlPrinter.o
	ar -rc $@ gtest-all.o gtest_main.o  HtmlPrinter.o 

install: $(TARGET)
	mkdir -p ${TEST_LIBDIR}/
	cp *.a *.so ${TEST_LIBDIR}/
	mkdir -p  $(TEST_HEADERDIR)/gtest/internal
	cp -r $(GTEST_DIR)/include/gtest/*.h $(TEST_HEADERDIR)/gtest/
	cp -r $(GTEST_DIR)/include/gtest/internal/*.h $(TEST_HEADERDIR)/gtest/internal

clean :
	rm -f $(TESTS)  *.a *.o *.so

