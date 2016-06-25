/*
 * MonitorSetter.h
 *Monitor ��������ͨ����̬ʵ�ֲ��õ����÷�ʽ��Ŀǰʵ����DEFAULT��ʽ��������ʽ
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
	//Ĭ��
	static boost::shared_ptr<MonitorSetter> factory();
	//�������
	static boost::shared_ptr<MonitorSetter> factory(int argc, char** argv);
	//�����ļ�
	static boost::shared_ptr<MonitorSetter> factory(string filename);

};

}
}

#endif /* MONITORSETTER_H_ */
