/*
 * HtmlPrinter.cpp
 *
 *  Created on: 2012-4-1
 *      Author: Administrator
 */

#include "gtest/HtmlPrinter.h"
namespace{
	using namespace  std;
	using namespace testing;
	std::string FormatTimeInMillisAsSeconds(TimeInMillis ms) {
		::std::stringstream ss;
		ss << ms/1000.0;
		return ss.str();
	}
	string StringStreamToString(::std::stringstream* ss) {
		const ::std::string& str = ss->str();
		const char* const start = str.c_str();
		const char* const end = start + str.length();

		// We need to use a helper stringstream to do this transformation
		// because String doesn't support push_back().
		::std::stringstream helper;
		for (const char* ch = start; ch != end; ++ch) {
			if (*ch == '\0') {
				helper << "\\0"; // Replaces NUL with "\\0";
			} else {
				helper.put(*ch);
			}
		}

		return string(helper.str().c_str());
	}

	void OutputHtmlTestInfo(::std::ostream* stream,
			const char* test_case_name,
			const TestInfo& test_info) {
		if(!test_info.should_run()){
			return;
		}
		const TestResult& result = *test_info.result();
		*stream<<"<tr>\n"
				<<"<td>"<<test_case_name<<"."<<test_info.name()<<"</td>\n"
				<<"<td><table>\n";
		for (int i = 0; i < result.total_part_count(); ++i) {
			const TestPartResult& part = result.GetTestPartResult(i);
			if(!part.type()==TestPartResult::kSuccess)
				*stream << "<tr><td>"<<part.message()<<"</td></tr>\n";
		}
		*stream	<<"</table></td>\n";
		*stream	<<"<td>"<<FormatTimeInMillisAsSeconds(result.elapsed_time()).c_str()<<"</td>\n";
		char * ok="OK";
		char * error="Error";
		*stream	<<"<td>"<<(result.Passed()?ok:error)<<"\n"<<"</td>";

	}
	void PrintHtmlTestCase(FILE* out,
		const TestCase& test_case) {
	for (int i = 0; i < test_case.total_test_count(); ++i) {
		::std::stringstream stream;
		OutputHtmlTestInfo(&stream, test_case.name(), *test_case.GetTestInfo(i));
		fprintf(out, StringStreamToString(&stream).c_str());
	}
}
	void PrintHtml(FILE* out,
			const UnitTest& unit_test) {
		fprintf(out, "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0 Transitional//EN\"> \n");
		fprintf(out, "<HTML> \n");
		fprintf(out, "<HEAD> \n");
		fprintf(out, "<TITLE>测试报告</TITLE> \n");
		fprintf(out, "</HEAD> \n");
		fprintf(out, "<body> \n");
		fprintf(out, "<table border=1 style=\"border: rgb(0,0,0) 1px \" cellpadding=\"0\" cellspacing=\"0\" > \n");
		fprintf(out, "<tr><th>测试用例名称</th><th>步骤</th><th>用时</th><th>结果</th></tr> \n");
		for (int i = 0; i < unit_test.total_test_case_count(); ++i){
			PrintHtmlTestCase(out, *unit_test.GetTestCase(i));
		}
		fprintf(out, "<table><tr><td>总数：%d</td> <td>实测：%d</td><td>成功：%d</td><td>失败：%d</td><td>用时:%s</td></tr></table>\n",
				unit_test.total_test_count(),
				unit_test.test_to_run_count(),
				unit_test.successful_test_count(),
				unit_test.failed_test_count(),
				FormatTimeInMillisAsSeconds(unit_test.elapsed_time()).c_str());
		fprintf(out, "</body> \n");
		fprintf(out, "</html> \n");
	}
}
namespace testing {
HtmlPrinter::HtmlPrinter(const char* output_file)
: output_file_(output_file) {
	if (output_file_.c_str() == NULL || output_file_.empty()) {
		fprintf(stderr, "html output file may not be null\n");
		fflush(stderr);
		exit(EXIT_FAILURE);
	}
}

void HtmlPrinter::OnTestIterationEnd(const UnitTest& unit_test,
		int /*iteration*/) {
	using namespace testing::internal;
	FILE* xmlout = NULL;
	FilePath output_file(output_file_);
	FilePath output_dir(output_file.RemoveFileName());

	if (output_dir.CreateDirectoriesRecursively()) {
		xmlout = posix::FOpen(output_file_.c_str(), "w");
	}
	if (xmlout == NULL) {
		fprintf(stderr,
				"Unable to open file \"%s\"\n",
				output_file_.c_str());
		fflush(stderr);
		exit(EXIT_FAILURE);
	}
	PrintHtml(xmlout, unit_test);
	fclose(xmlout);
}
}
