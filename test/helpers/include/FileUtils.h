#include <string>
#include <map>
#include "Config.h"
namespace ai {
namespace test {
using namespace std;
class AttributeProvider {
public:
	AttributeProvider(map<string,string > &	context,const string & configfile = "")
		:context(context) {
		if (configfile!=""&&!configfile.empty()){
			config = new Config(configfile);
		}else {
			config = 0;
		}
	}
	~AttributeProvider(){
		if(config)
			delete config;
	}
	string get(const string &key) {
		map<string, string>::iterator iter;
		iter = context.find(key);
		if (iter != context.end()) {
			return iter->second;
		} else if(config&&config->KeyExists(key)){
				return config->Read<string>(key);
		}else{
			if (char * p=getenv(key.c_str())) {
				return p;
			} else {
				return "";
			}
		}
	}
private:
	map<string, string> context;
	Config *config;

};

class FileUtils {
public:
	//�ļ��Ƿ����
	static bool exists(const string  filname);
	//�ļ����Ƿ��������
	static bool contain(const string  filname, const string  content);
	//ɾ���ļ�
	static bool del(const string  filename);
	//ͨ��ģ�������ļ�
	static void  copy(const string  src,const string  dest);
	static bool genfile(const string tempfile, const string  outfile,
			map<string, string> &context,const string  configfile="");
	static string regreplace(string &src, AttributeProvider *attributeProvider=(AttributeProvider *)0);
};
}
}

