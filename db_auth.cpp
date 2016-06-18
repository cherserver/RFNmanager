#include <api/module.h>
#include <api/auth_method.h>
#include <mgr/mgrstr.h>
#include "tables.h"

namespace rfnpriv_db_auth {
using namespace isp_api;

class SimpleAuth : public isp_api::AuthMethod {
public:
	SimpleAuth() : isp_api::AuthMethod("simple") {}

	void Fillup(mgr_xml::Xml &xml, std::shared_ptr<rfndb::User> &user) const {
		auto node = xml.GetRoot().AppendChild("ok");
		node.SetProp("level", str::Str(lvAdmin));
		node.SetProp("name", user->Name);
		node.SetProp("method", this->name());
		node.AppendChild("ext", user->Id).SetProp("name", "uid");

	}
	virtual void AuthenByName(mgr_xml::Xml &res, const string &name) const {
		auto cur = rfndb::GetCache()->Get<rfndb::User>();
		if (cur->FindByName(name) && cur->Active)
			Fillup(res, cur);
	}
	virtual void AuthenByPassword(mgr_xml::Xml &res, const string &name, const string &pass) const {
		auto cur = rfndb::GetCache()->Get<rfndb::User>();
		if (cur->FindByName(name) && cur->Active && str::Crypt(pass, cur->Password) == cur->Password)
			Fillup(res, cur);
	}
	virtual bool Find(const string &name) const {
		auto cur = rfndb::GetCache()->Get<rfndb::User>();
		return cur->FindByName(name);
	}
	virtual void SetPassword(const string &user, const string &pass) const {
		auto cur = rfndb::GetCache()->Get<rfndb::User>();
		cur->AssertByName(user);
		cur->Password = str::Crypt(pass);
		cur->Post();
	}
};

MODULE_INIT(auth, "rfndb") {
	new SimpleAuth;
}
} // end of rfnpriv_db_auth namespace

