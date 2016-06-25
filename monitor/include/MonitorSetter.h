/*
 * MonitorSetter.h
 *Monitor 设置器，通过多态实现不用的配置方式，目前实现了DEFAULT方式、参数方式
 *  Created on: 2012-2-23
 *      Author: renyz
 */

#ifndef MONITORSETTER_H_
#define MONITORSETTER_H_
#include "SGMonitor.h"
namespace ai {
namespace sg {

class MonitorSetter {
protected:
	const static	int port;
	const static	string transport_type;
	const static	string protocol_type;
	const static	string server_type;
	const static	size_t workers;
public:
	MonitorSetter();
	virtual ~MonitorSetter();
	virtual int set(Monitor & monitor)=0;
	//默认
	static boost::shared_ptr<MonitorSetter> factory();
	//命令参数
	static boost::shared_ptr<MonitorSetter> factory(int argc, char** argv);
	//配置文件
	static boost::shared_ptr<MonitorSetter> factory(string filename);

};

}
}

#endif /* MONITORSETTER_H_ */
