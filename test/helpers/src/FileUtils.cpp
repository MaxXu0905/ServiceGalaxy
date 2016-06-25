#include "FileUtils.h"
#include  <iostream>
#include  <fstream>
#include <stdlib.h>
#include <boost/filesystem.hpp>
#include "myregexp.h"
namespace fs = boost::filesystem;
namespace ai {
namespace test {
using namespace boost;
//文件是否存在
bool FileUtils::exists(const string  filname) {
	return fs::exists(fs::path(filname));
}
//文件中是否包含内容
bool FileUtils::contain(const string  filname, const string  content) {
	if (!exists(filname)) {
		return false;
	}
	ifstream fin(filname.c_str());
	if (!fin.good()) {
		return false;
	}
	string line;
	while (getline(fin, line)) {

		if (line.find(content, 0) != string::npos) {
			return true;
		}
	}
	return false;

}
//删除文件
bool FileUtils::del(const string  filename) {
	return fs::remove(fs::path(filename));

}
void FileUtils::copy(const string  src,const string  dest){
	fs::copy_file(fs::path(src),fs::path(dest),fs::copy_option::overwrite_if_exists);
}
//通过模板生成文件
bool FileUtils::genfile(const string tempfile, const string outfile,
		map<string, string> &context,const string configfile) {
	ifstream fin(tempfile.c_str());
	ofstream fout(outfile.c_str());
	if (!fin.good() || !fout.good()) {
		return false;
	}
	AttributeProvider attributeProvider(context,configfile);
	string line;
	while (getline(fin, line)) {
		fout << FileUtils::regreplace(line, &attributeProvider) << endl;
	}
	return true;
}
namespace {
CRegexpT<char> tempregexp("\\$\\{([^}]*)\\}");
int tempregexpGroup = 1;
}
string FileUtils::regreplace(string &src,AttributeProvider *attributeProvider) {
	const char * text = src.c_str();
	MatchResult result = tempregexp.Match(text);
	string strresult = src;
	while (result.IsMatched()) {
		string attribute(
				text + result.GetGroupStart(tempregexpGroup),
				result.GetGroupEnd(tempregexpGroup)
						- result.GetGroupStart(tempregexpGroup));
		strresult.replace(result.GetStart(),
						result.GetEnd() - result.GetStart(), attributeProvider->get(attribute));
		text=strresult.c_str();
		result = tempregexp.Match(text, result.GetEnd());
	}
	return strresult;
}

}
}
