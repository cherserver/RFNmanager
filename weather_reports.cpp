#include <mgr/mgrlog.h>
#include <api/action.h>
#include <api/dbaction.h>
#include <api/module.h>
#include "tables.h"

MODULE("weather_reports");

namespace rfnpriv_weather_reports {
using namespace isp_api;

StringPair GetPeriod(Session &ses, const string &fld_name) {
	string date_begin = ses.IParam(fld_name + "start");
	string date_end = ses.IParam(fld_name + "end");
	if (date_begin.empty() || date_end.empty())
		throw mgr_err::Value(fld_name, ses.Param(fld_name));

	StringPair vals;
	vals.first = str::Str((time_t)mgr_date::DateTime(date_begin + " 00:00:00"));
	vals.second = str::Str((time_t)mgr_date::DateTime(date_end + " 23:59:59"));
	return vals;
}

string ToDateFormat(time_t dateval, bool has_time) {
	return mgr_date::Format(dateval, has_time ? "%Y.%m.%d %H:%M" : "%Y.%m.%d");
}

class CommonReport : public ReportAction {
public:
	CommonReport(const string &action_name, const string &table_suffix)
	: ReportAction(action_name, MinLevel(lvAdmin))
	, m_table_suffix(table_suffix) {}
private:
	string m_table_suffix;

	void FillWeatherByType(const string &type, mgr_xml::XmlNode &band_node, const string &begin, const string &end) const {
		string n_type = "_" + str::Lower(type);
		ForEachQuery(rfndb::GetCache(),
			"SELECT begin"	//0
			", temperature"	//1
			", humidity"	//2
			" FROM dht_" + m_table_suffix +
			" WHERE datatype = " + rfndb::Escape(type) +
				" AND begin >= " + rfndb::Escape(begin) +
				" AND begin <= " + rfndb::Escape(end)
			, q) {
			auto elem = band_node.AppendChild("elem");
			elem.AppendChild(string("begin" + n_type).c_str(), ToDateFormat(mgr_date::DateTime(q->AsInt(0)), m_table_suffix == "stathr"));
			elem.AppendChild(string("temperature" + n_type).c_str(), str::Str(q->AsDouble(1), 1));
			elem.AppendChild(string("humidity" + n_type).c_str(), str::Str(q->AsDouble(2), 1));
		}
	}

	virtual void ReportExecute(Session &ses) const {
		auto repdata = ses.NewNode("reportdata");

		auto rep_period = GetPeriod(ses, "period");

		//indoor_weather
		auto iw_band = repdata.AppendChild("indoor_weather");
		FillWeatherByType("I", iw_band, rep_period.first, rep_period.second);

		//outdoor_weather
		auto ow_band = repdata.AppendChild("outdoor_weather");
		FillWeatherByType("O", ow_band, rep_period.first, rep_period.second);

		auto p_band = repdata.AppendChild("pressure");
		ForEachQuery(rfndb::GetCache(),
			"SELECT begin"
			", pressure"
			" FROM pressure_" + m_table_suffix +
			" WHERE datatype = 'C'"
				" AND begin >= " + rfndb::Escape(rep_period.first) +
				" AND begin <= " + rfndb::Escape(rep_period.second)
			, q) {
			auto elem = p_band.AppendChild("elem");
			elem.AppendChild("begin_p", ToDateFormat(mgr_date::DateTime(q->AsInt(0)), m_table_suffix == "stathr"));
			elem.AppendChild("pressure", str::Str(q->AsDouble(1), 1));
		}
	}
};

class DHTDataReport : public ReportAction {
public:
	DHTDataReport(const string &action_name, const string &table_suffix)
	: ReportAction(action_name, MinLevel(lvAdmin))
	, m_table_suffix(table_suffix) {}
private:
	string m_table_suffix;
	virtual void ReportFormTune(Session &ses) const {
		DbSelect(ses
				, "weather_type"
				, rfndb::GetCache()->Query("SELECT id, name FROM weather_types")
		);
	}

	virtual void ReportExecute(Session &ses) const {
		auto wtype_cur = rfndb::GetCache()->Get<rfndb::WeatherTypes>(ses.Param("weather_type"));
		auto repdata = ses.NewNode("reportdata");
		auto rep_period = GetPeriod(ses, "period");

		auto w_band = repdata.AppendChild("dhtdata");
		ForEachQuery(rfndb::GetCache(),
			"SELECT begin"	//0
			", temperature"	//1
			", humidity"	//2
			" FROM dht_" + m_table_suffix +
			" WHERE datatype = " + rfndb::Escape(wtype_cur->MsgType) +
				" AND begin >= " + rfndb::Escape(rep_period.first) +
				" AND begin <= " + rfndb::Escape(rep_period.second)
			, q) {
			auto elem = w_band.AppendChild("elem");
			elem.AppendChild("begin", ToDateFormat(mgr_date::DateTime(q->AsInt(0)), m_table_suffix == "stathr"));
			elem.AppendChild("temperature", str::Str(q->AsDouble(1), 1));
			elem.AppendChild("humidity", str::Str(q->AsDouble(2), 1));
		}
	}
};

MODULE_INIT(weather_reports, "stats") {
	new CommonReport("weather_hourly", "stathr");
	new CommonReport("weather_daily", "statday");
	new DHTDataReport("dhtdata_hourly", "stathr");
	new DHTDataReport("dhtdata_daily", "statday");
}
} // end of rfnpriv_weather_reports namespace
