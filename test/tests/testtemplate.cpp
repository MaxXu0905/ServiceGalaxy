#include <gtest/gtest.h>
#include <iostream>
#define _TEST_FILE_ testtitle
//g++ -I../include -L../lib -lgtest testtemplate.cpp 
using namespace std;
using namespace testing;
class MyEnvironment: public testing::Environment {
public:
	virtual void SetUp() {
		   cout<<"MyEnvironment setup"<<endl;
	}
	virtual void TearDown() {
		cout<<"MyEnvironment TearDown"<<endl;
	}
};
class xxx:public Test{
public:
	  static void SetUpTestCase() {

	cout<<"SetUpTestCase"<<endl;
	}

	static void TearDownTestCase() {
		  cout<<"TearDownTestCase"<<endl;
	}
protected:
	  virtual void SetUp(){
	cout<<"SetUp"<<endl;
		  
	  }

	  virtual void TearDown(){
		cout<<"TearDown"<<endl;
	  }
};
TEST_F3(xxx,casename,testname){
	cout<<"reu testbody"<<endl;
}
TEST_F3(xxx,casename,testname2){
        cout<<"reu testbody2"<<endl;
}
TEST_F4(xxx,testcase4){
	cout<<"reu testbody4"<<endl;
}
int main(int argc,char * argv[]){
	AddGlobalTestEnvironment(new MyEnvironment);
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
