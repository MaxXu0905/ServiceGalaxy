#include "sdc_sqlsvc.h"

using namespace std;
using namespace ai::scci;

namespace ai
{
namespace sg
{

sdc_sqlsvc::sdc_sqlsvc()
	: api_mgr(dbc_api::instance())
{
	DBCT = dbct_ctx::instance();
	dbc_db = new Dbc_Database();
}

sdc_sqlsvc::~sdc_sqlsvc()
{
	delete dbc_db;
}

svc_fini_t sdc_sqlsvc::svc(message_pointer& svcinfo)
{
	if (svcinfo->length() < sizeof(sdc_sql_rqst_t)) {
		svcinfo->assign((_("ERROR: invalid message given")).str(SGLOCALE));
		return svc_fini(SGSUCCESS, 0, svcinfo);
	}

	sdc_sql_rqst_t *rqst = reinterpret_cast<sdc_sql_rqst_t *>(svcinfo->data());

	// ÇÐ»»ÓÃ»§
	if (!api_mgr.set_user(rqst->user_id)) {
		svcinfo->assign((_("ERROR: invalid user")).str(SGLOCALE));
		return svc_fini(SGSUCCESS, 0, svcinfo);
	}

	ostringstream fmt;
	string str = rqst->placeholder;
	if (strncasecmp(str.c_str(), "select", 6) != 0) {
		svcinfo->assign((_("ERROR: not a select statement")).str(SGLOCALE));
		return svc_fini(SGSUCCESS, 0, svcinfo);
	}

	int i;
	vector<string> the_vector;
	string field;
	for (string::const_iterator iter = str.begin(); iter != str.end(); ++iter) {
		if (isspace(*iter) || *iter == ';') {
			if (field.empty())
				continue;

			the_vector.push_back(field);
			field = "";
		} else {
			field += *iter;
		}
	}
	if (!field.empty())
		the_vector.push_back(field);

	string sql;
	if (the_vector.size() >= 3 && the_vector[0] == "*" && strcasecmp(the_vector[1].c_str(), "from") == 0) {
		dbc_te& te_mgr = dbc_te::instance(DBCT);
		dbc_te_t *te = te_mgr.find_te(the_vector[2]);
		if (te == NULL) {
			svcinfo->assign((_("ERROR: object {1} does not exist.") % the_vector[2]).str(SGLOCALE));
			return svc_fini(SGSUCCESS, 0, svcinfo);
		}

		dbc_data_t& te_meta = te->te_meta;
		for (int j = 0; j < te_meta.field_size; j++) {
			if (j == 0)
				sql += " ";
			else
				sql += ", ";
			sql += te_meta.fields[j].field_name;
		}

		// Skip over first *
		for (i = 0; i < str.length() && str[i] != '*'; i++)
			;

		sql += " ";
		sql += str.substr(i + 1);
	} else {
		sql = str;
	}

	try {
		auto_ptr<Generic_Statement> auto_stmt(dbc_db->create_statement());
		Generic_Statement *stmt = auto_stmt.get();

		auto_ptr<struct_dynamic> auto_data(dbc_db->create_data());
		struct_dynamic *data = auto_data.get();
		if (data == NULL) {
			svcinfo->assign((_("ERROR: Memory allocation failure")).str(SGLOCALE));
			return svc_fini(SGSUCCESS, 0, svcinfo);
		}

		data->setSQL(sql);
		stmt->bind(data);

		auto_ptr<Generic_ResultSet> auto_rset(stmt->executeQuery());
		Generic_ResultSet *rset = auto_rset.get();

		// Write out header
		for (i = 1; i <= rset->getColumnCount(); i++) {
			if (i > 1)
				fmt << " ";
			sqltype_enum column_type = rset->getColumnType(i);
			if (column_type == SQLTYPE_DATE || column_type == SQLTYPE_TIME || column_type == SQLTYPE_DATETIME)
				fmt << std::setw(14);
			fmt << std::left << rset->getColumnName(i) << std::right;
		}
		fmt << "\n";

		for (i = 1; i <= rset->getColumnCount(); i++) {
			sqltype_enum column_type = rset->getColumnType(i);

			fmt << std::setfill('-');
			if (column_type == SQLTYPE_DATE || column_type == SQLTYPE_TIME || column_type == SQLTYPE_DATETIME)
				fmt << std::setw(15);
			else
				fmt << std::setw(rset->getColumnName(i).length() + 1);
			fmt << ' ' << std::setfill(' ');
		}
		fmt << "\n";

		long affected_rows = 0;
		while (rset->next()) {
			for (i = 1; i <= rset->getColumnCount(); i++) {
				if (i > 1)
					fmt << " ";

				sqltype_enum column_type = rset->getColumnType(i);
				if (column_type == SQLTYPE_DATE || column_type == SQLTYPE_TIME || column_type == SQLTYPE_DATETIME)
					fmt << std::setw(14);
				else
					fmt << std::setw(rset->getColumnName(i).length());
				fmt << std::left << rset->getString(i) << std::right;
			}
			fmt << "\n";
			if (++affected_rows >= rqst->max_rows)
				break;
		}

		if (affected_rows > 1)
			fmt << "\n" << affected_rows << (_(" records selected successfully.\n")).str(SGLOCALE);
		else
			fmt << "\n" << affected_rows << (_(" record selected successfully.\n")).str(SGLOCALE);

		svcinfo->assign(fmt.str());
		return svc_fini(SGSUCCESS, 0, svcinfo);
	} catch (exception& ex) {
		svcinfo->assign(ex.what());
		return svc_fini(SGSUCCESS, 0, svcinfo);
	}
}

}
}

