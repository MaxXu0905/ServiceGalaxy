/*
 * HtmlPrinter.h
 *
 *  Created on: 2012-4-1
 *      Author: Administrator
 */

#ifndef HTMLPRINTER_H_
#define HTMLPRINTER_H_
#include "gtest.h"
namespace testing {


class HtmlPrinter : public EmptyTestEventListener {
public:
	explicit HtmlPrinter(const char* output_file);

	virtual void OnTestIterationEnd(const UnitTest& unit_test, int iteration);
private:
	const std::string output_file_;
	GTEST_DISALLOW_COPY_AND_ASSIGN_(HtmlPrinter);
};
}
#endif /* HTMLPRINTER_H_ */
