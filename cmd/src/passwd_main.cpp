#include <cstdio>
#include <signal.h>
#include <unistd.h>
#include <cstring>
#include <strings.h>
#include <fcntl.h>
#include <unistd.h>
#include <dlfcn.h>
#include <exception>
#include <iostream>
#include "common.h"

using namespace std;
using namespace ai::sg;

int main(int argc,char** argv)
{
	try {
		if (argc != 2) {
			cout << (_("Usage:\n")).str(SGLOCALE);
			cout << "sgpasswd password\n" << std::endl;
			exit(1);
		}

		char cipher[1024];
		common::encrypt(argv[1], cipher);
		cout << (_("cipher")).str(SGLOCALE) << " = [" << cipher << "]" << std::endl;
		return 0;
	} catch (exception& ex) {
		cout << ex.what() << std::endl;
		return 1;
	}
}

