#include "tables.h"
#include <api/module.h>
#include <api/dbaction.h>

namespace rfndb {
mgr_db::ConnectionParams GetConnectionParams() {
	mgr_db::ConnectionParams params;
	params.type = "sqlite";
	params.db = "etc/rfnmgr.db";
	params.client = "rfnmgr";
	return params;
}

mgr_db::JobCache *cache = nullptr;

mgr_db::JobCache * GetCache() {
	ASSERT(cache != nullptr, "Try to use rfndb before it was inited. Add rfndb to your module dependencies");
	return cache;
}

string Escape(const string &value) {
	return GetCache()->EscapeValue(value);
}

User::User()
	: Table("user", 63)
	, Active(this, "active")
	, Password(this, "passwd", 128) {
	Active.info().set_default("on");
	Password.info().access_read = mgr_access::AccessNone;
}

WeatherTypes::WeatherTypes()
	: Table("weather_types")
	, Main(this, "main")
	, Outdoor(this, "outdoor")
	, MsgType(this, "msg_type", 8) {
	AddIndex(mgr_db::itUnique, MsgType);
}
} //end of rfndb namespace

namespace rfnpriv_tables {
using namespace isp_api;
class ActionWeatherTypes : public TableNameListAction<rfndb::WeatherTypes> {
public:
	ActionWeatherTypes() : TableNameListAction("weather_types", MinLevel(lvAdmin), *rfndb::GetCache()) {}
};

MODULE_INIT(rfndb, "") {
	mgr_db::ConnectionParams params;
	params.type = "sqlite";
	params.db = "etc/rfnmgr.db";
	params.client = "rfnmgr";
	RegisterComponent(rfndb::cache = new mgr_db::JobCache(rfndb::GetConnectionParams()));
	rfndb::GetCache()->Register<rfndb::User>();
	rfndb::GetCache()->Register<rfndb::WeatherTypes>();
	new ActionWeatherTypes;
}
} // end of rfnpriv_tables namespace

