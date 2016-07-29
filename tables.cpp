#include "tables.h"
#include <api/module.h>

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
User::User()
	: Table("user", 63)
	, Active(this, "active")
	, Password(this, "passwd", 128) {
	Active.info().set_default("on");
	Password.info().access_read = mgr_access::AccessNone;
}
} //end of rfndb namespace

namespace rfnpriv_tables {
MODULE_INIT(rfndb, "") {
	mgr_db::ConnectionParams params;
	params.type = "sqlite";
	params.db = "etc/rfnmgr.db";
	params.client = "rfnmgr";
	isp_api::RegisterComponent(rfndb::cache = new mgr_db::JobCache(rfndb::GetConnectionParams()));
	rfndb::GetCache()->Register<rfndb::User>();
}
} // end of rfnpriv_tables namespace

