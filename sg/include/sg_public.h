#if !defined(__SG_PUBLIC_H__)
#define __SG_PUBLIC_H__

#include "sg_struct.h"
#include "sg_align.h"
#include "sg_exception.h"
#include "sg_message.h"
#include "sg_svc.h"
#include "sg_api.h"
#include "log_file.h"
#include "perf_stat.h"

#include <ctime>
#include <cmath>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <string>
#include <map>
#include <vector>
#include <algorithm>
#include <signal.h>

#include <boost/format.hpp>
#include <boost/foreach.hpp>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/shared_array.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <boost/interprocess/sync/sharable_lock.hpp>
#include <boost/interprocess/ipc/message_queue.hpp>
#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/sync/interprocess_mutex.hpp>
#include <boost/interprocess/sync/interprocess_recursive_mutex.hpp>
#include <boost/interprocess/sync/interprocess_upgradable_mutex.hpp>
#include <boost/interprocess/managed_mapped_file.hpp>
#include <boost/interprocess/exceptions.hpp>
#include <boost/io/ios_state.hpp>
#include <boost/tokenizer.hpp>
#include <boost/scoped_array.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/program_options/cmdline.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/variables_map.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/errors.hpp>
#include <boost/thread.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/filesystem.hpp>
#include <boost/scope_exit.hpp>
#include <boost/functional/hash.hpp>
#include <boost/crc.hpp>
#include <boost/cstdint.hpp>
#include <boost/chrono.hpp>
#include <boost/timer.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/random/linear_congruential.hpp>

template<typename T>
T get_value(const boost::property_tree::iptree& pt, const boost::property_tree::iptree::path_type& path, const T& default_value) {
	boost::optional<const boost::property_tree::iptree&> node = pt.get_child_optional(path);
	if (!node)
		return default_value;
	else
		return node->get_value<T>();
}

#endif

