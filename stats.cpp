#include <mgr/mgrfile.h>
#include <api/action.h>
#include <api/autotask.h>
#include <api/module.h>
#include <api/stdconfig.h>
#include <ostemplate/stat_raw.h>
#include <ostemplate/stat_utils.h>
#include "tables.h"
#include "radio.h"

#define STAT_YEARS_PARAM "YearsOfStat"
#define PROCESS_STAT_ACTION "process_stat"

namespace rfnpriv_stats {
using namespace isp_api;
using namespace Stat;

std::shared_ptr<mgr_db::JobCache> stat_cache;

class AvgDataField : public DataField {
public:
	AvgDataField(Recordset *rs, const string &name)
	: DataField(rs, name, false, dftAvg)
	, m_DoubleValue(0)
	, m_fold_counter(0) {}

	mgr_db::Field *CreateDbField(mgr_db::CustomTable *table) {
		return new mgr_db::DoubleField (table, Name());
	}

	string GetFun() {
		return "avg(" + Name() + ") " + Name();
	}

	void Set(const string &value) {
		m_DoubleValue = str::Double(value);
	}

	string Value() const {
		return str::Str(m_DoubleValue, 3);
	}

	AvgDataField &operator =(double data) {
		m_DoubleValue = data;
		return *this;
	}

	void Fold(Field *other) {
		double norm_val = m_DoubleValue * m_fold_counter;
		++m_fold_counter;
		norm_val += str::Double(other->Value());

		m_DoubleValue = norm_val / m_fold_counter;
	}
private:
	double m_DoubleValue;
	unsigned long long m_fold_counter;
};

class DHTStatRecordset : public Recordset {
public:
	KeyField DataType;
	AvgDataField Temperature;
	AvgDataField Humidity;

	virtual string Name() const { return "dhtstat"; }

	DHTStatRecordset()
	: DataType(this, "datatype")
	, Temperature(this, "temperature")
	, Humidity(this, "humidity") {}
};

class DHTHourlyStatTable : public DataTable<DHTStatRecordset> {
public:
	DHTHourlyStatTable() : DataTable<DHTStatRecordset>("dht_stathr", 24 * 365 * str::Int(mgr_cf::GetParam(STAT_YEARS_PARAM))) {}
};

class DHTDailyStatTable : public DataTable<DHTStatRecordset> {
public:
	DHTDailyStatTable() : DataTable<DHTStatRecordset>("dht_statday", 365 * str::Int(mgr_cf::GetParam(STAT_YEARS_PARAM))) {}
};

class PressureStatRecordset : public Recordset {
public:
	KeyField DataType;
	AvgDataField Pressure;

	virtual string Name() const { return "pressurestat"; }

	PressureStatRecordset()
	: DataType(this, "datatype")
	, Pressure(this, "pressure") {}
};

class PressureHourlyStatTable : public DataTable<PressureStatRecordset> {
public:
	PressureHourlyStatTable() : DataTable<PressureStatRecordset>("pressure_stathr", 24 * 365 * str::Int(mgr_cf::GetParam(STAT_YEARS_PARAM))) {}
};

class PressureDailyStatTable : public DataTable<PressureStatRecordset> {
public:
	PressureDailyStatTable() : DataTable<PressureStatRecordset>("pressure_statday", 365 * str::Int(mgr_cf::GetParam(STAT_YEARS_PARAM))) {}
};

class ProcessStat : public Action {
public:
	ProcessStat() : Action(PROCESS_STAT_ACTION, MinLevel(lvSuper)) {}

	virtual bool IsModify(const Session &ses) const {
		return false;
	}

private:
	virtual void Execute(Session &ses) const {
		mgr_date::AccurateDateTime now;

		HourlyProcess<DHTHourlyStatTable>(now);
		DailyProcess<DHTDailyStatTable, DHTHourlyStatTable>(now);

		HourlyProcess<PressureHourlyStatTable>(now);
		DailyProcess<PressureDailyStatTable, PressureHourlyStatTable>(now);
	}
};

class DHTDataEvent : public Event {
public:
	DHTDataEvent() : Event(DHTDATA_ACTION, DHTDATA_ACTION ".stat") {}
private:
	virtual void AfterExecute(Session &ses) const {
		DHTStatRecordset dhtstat;
		dhtstat.DataType = ses.Param("msgtype");
		dhtstat.Temperature = str::Double(ses.Param("temp"));
		dhtstat.Humidity = str::Double(ses.Param("hum"));

		dhtstat.Normalize();
		MinutelyProcess(mgr_date::AccurateDateTime(), dhtstat);
	}
};

class PressureDataEvent : public Event {
public:
	PressureDataEvent() : Event(PRESSURE_ACTION, PRESSURE_ACTION ".stat") {}
private:
	virtual void AfterExecute(Session &ses) const {
		PressureStatRecordset pres_stat;
		pres_stat.DataType = "C"; //common
		pres_stat.Pressure = str::Double(ses.Param("value"));

		pres_stat.Normalize();
		MinutelyProcess(mgr_date::AccurateDateTime(), pres_stat);
	}
};

MODULE_INIT(stats, "radio") {
	mgr_cf::AddParam(STAT_YEARS_PARAM, "10");
	stat_cache.reset(new mgr_db::JobCache(rfndb::GetConnectionParams()));
	StatUtils::InitStatsDB(stat_cache);

	stat_cache->Register<DHTHourlyStatTable>();
	stat_cache->Register<DHTDailyStatTable>();
	stat_cache->Register<PressureHourlyStatTable>();
	stat_cache->Register<PressureDailyStatTable>();

	new DHTDataEvent;
	new PressureDataEvent;
	new ProcessStat;
	isp_api::task::Schedule(
			mgr_file::ConcatPath(mgr_file::GetCurrentDir(), "sbin/mgrctl")
				+ " -m " + isp_api::GetMgrName() + " " PROCESS_STAT_ACTION
			, "8 * * * *", "RFNmanager task to collect hourly statistics"
	);
}
} //end of rfnpriv_stats namespace
