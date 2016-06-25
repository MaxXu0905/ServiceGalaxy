/*
 * Log.cpp
 *
 *  Created on: 2012-2-22
 *      Author: renyz
 */
#include "MonitorLog.h"
#include "gpp_ctx.h"
namespace ai{
	namespace sg{
			void MonitorLog::write_log(const char *filename, int linenum, const char *msg, int len){
				//std::cout<<msg<<std::endl;
				gpp_ctx::instance()->write_log(filename,linenum,msg, len);
			}
			void MonitorLog::write_log(const char *filename, int linenum, const std::string& msg){
				//std::cout<<msg<<std::endl;
				gpp_ctx::instance()->write_log(filename, linenum, msg);
			}
			void MonitorLog::write_log(const std::string& msg){
				//std::cout<<msg<<std::endl;
				gpp_ctx::instance()->write_log(msg);
			}
			void MonitorLog::write_log(const char *msg){
				//std::cout<<msg<<std::endl;
				gpp_ctx::instance()->write_log(msg);
			}
	}
}

