/*
 * Log.h
 * 日志封装类，降低对SG的依赖
 *
 *  Created on: 2012-2-22
 *      Author: renyz
 */

#ifndef MONITORlOG_H
#define MONITORlOG_H
#include "monitorpch.h"
namespace ai {
namespace sg {
class MonitorLog {
public:
	static void write_log(const char *filename, int linenum, const char *msg,
			int len = 0);
	static void write_log(const char *filename, int linenum,
			const std::string & msg);
	static void write_log(const std::string& msg);
	static void write_log(const char *msg);

};
}
}

#endif /* MONITORlOG_H */
