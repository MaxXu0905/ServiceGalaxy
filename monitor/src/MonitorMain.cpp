/*
 * MonitorMain.cpp
 * 独立起起TGWS类（用于测试）
 *  Created on: 2012-2-15
 *      Author: renyz
 */

#include "SGMonitor.h"
#include "monitorpch.h"
#include "MonitorSetter.h"
#include "sg_internal.h"
using namespace ai::sg;
using namespace std;
int main(int argc, char** argv) {
	sg_api& api_mgr = sg_api::instance();
		if (!api_mgr.init(NULL)) {
			std::cout << (_("ERROR: Can't login to BB, error_code = ")).str(SGLOCALE)  << std::endl;
			exit(1);
		}

	Monitor m;
	MonitorSetter::factory()->set(m);
	if(MonitorSetter::factory(argc,argv)->set(m)){
		std::cout<<(_("set fails")).str(SGLOCALE)<<endl;
		return -1;
	}
	cout<<(_("m return")).str(SGLOCALE)<<m.run()<<endl;
	std::cout<<(_("started")).str(SGLOCALE)<<std::endl;




}

