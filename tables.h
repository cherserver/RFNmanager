#ifndef __TABLES_H__
#define	__TABLES_H__
#include <mgr/mgrdb_struct.h>

namespace rfndb {
mgr_db::ConnectionParams GetConnectionParams();
mgr_db::JobCache * GetCache();
string Escape(const string &value);

class User : public mgr_db::Table {
public:
	mgr_db::BoolField Active;
	mgr_db::StringField Password;

	User();
};

class WeatherTypes : public mgr_db::Table {
public:
	mgr_db::BoolField Main;
	mgr_db::BoolField Outdoor;
	mgr_db::StringField MsgType;

	WeatherTypes();
};
} //end of rfndb namespace

#endif

