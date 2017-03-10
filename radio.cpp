#include "radio.h"
#include <mgr/mgrclient.h>
#include <mgr/mgrlog.h>
#include <mgr/mgrstr.h>
#include <mgr/mgrthread.h>
#include <mgr/mgrproc.h>
#include <api/action.h>
#include <api/module.h>
#include <api/autotask.h>

#include <RF24/RF24.h>
#include <RF24Network/RF24Network.h>

#define SEND_TIME_ACTION "sendtime"

//RADIO_NETWORK
#define RADIO_CHANNEL 55
#define RADIO_REC_MASTER 0
#define RADIO_REC_TABLE_CLOCK 01
#define RADIO_REC_OUTER_RADIO 02  //<<<
#define RADIO_REC_BATH_ALARM 03
#define BROADCAST_LEVEL 1

#define NETWORK_DELAY 50

MODULE("radio");

namespace rfnpriv_radio {
using namespace isp_api;

class RadioProcessor {
public:
	RadioProcessor()
	: m_radio(RegisterComponent(new RF24(RPI_V2_GPIO_P1_15, BCM2835_SPI_CS0, BCM2835_SPI_SPEED_4MHZ)))
	, m_network(nullptr)
	, m_init(false)
	, m_stop(false)
	, m_can_call(false)
	, m_send_time(false)
	, m_thread(mgr_thread::RunIt(this)) {
		Init();
	}

	~RadioProcessor() {
		m_stop = true;
		m_thread.join();
	}

	void SendTime() {
		m_send_time = true;
	}

	void Init() {
		if (m_init)
			return;
		if (!m_radio->begin()) {
			Warning("Radio is not initialized");
		} else {
			LogExtInfo("Radio successfully initialized");
		}

		m_network = RegisterComponent(new RF24Network(*m_radio));
		m_network->begin(RADIO_CHANNEL, RADIO_REC_MASTER);
		m_init = true;
	}

	void SetIsCallable() {
		Debug("Now radio actions can be called");
		m_can_call = true;
	}

	void operator() () {
		while (true) {
			if (m_stop)
				return;
			if (!m_init)
				continue;
			try {
				m_network->update();
				while (m_network->available()) {
					RF24NetworkHeader header;
					m_network->peek(header);
					switch (header.type) {
						case 'A': HandleAlarm(header); break;
						case 'B': HandleDHT(header.type, header); break;
						case 'C': HandleDHT(header.type, header); break;
						case 'I': HandleDHT(header.type, header); break;
						case 'O': HandleDHT(header.type, header); break;
						case 'P': HandlePressure(header); break;
						case 'R': HandleButton(header); break;
						default:
							m_network->read(header, 0, 0);
							Warning("Unknown radio message with type '%c' come", header.type);
							break;
					};
				}

				if (m_send_time) {
					RealSendTime();
					m_send_time = false;
				}

				mgr_proc::Sleep(NETWORK_DELAY);
			} catch (const mgr_err::Error &e) {
				Warning("Exception in radio process thread: %s", e.what());
			} catch(...) {
				Warning("Unknown Exception in radio process thread");
			}
		}
	}

	bool IsCallable() const {
		return m_can_call;
	}

	void HandleAlarm(RF24NetworkHeader& header) const {
		unsigned char message;
		m_network->read(header, &message, sizeof(message));
		if (!IsCallable())
			return;
		mgr_client::Local cli(MGR, "radio_data");
		cli.AddParam("alarm", string(1, message));
		cli.Query("func=" ALARM_ACTION);
	}

	void HandleButton(RF24NetworkHeader& header) const {
		uint8_t message;
		m_network->read(header, &message, sizeof(message));
		if (!IsCallable())
			return;
		mgr_client::Local cli(MGR, "radio_data");
		cli.AddParam("button", str::Str(message));
		cli.Query("func=" BUTTON_ACTION);
	}

	void HandleDHT(unsigned char msg_type, RF24NetworkHeader& header) const {
		float DHTData[2] = { 0, 0 };
		m_network->read(header, &DHTData, sizeof(DHTData));
		if (!IsCallable())
			return;
		mgr_client::Local cli(MGR, "radio_data");
		cli.AddParam("msgtype", string(1, msg_type));
		cli.AddParam("temp", str::Str(DHTData[0], 3));
		cli.AddParam("hum", str::Str(DHTData[1], 3));
		cli.Query("func=" DHTDATA_ACTION);
	}

	void HandlePressure(RF24NetworkHeader& header) const {
		uint16_t message;
		m_network->read(header, &message, sizeof(message));
		if (!IsCallable())
			return;
		mgr_client::Local cli(MGR, "radio_data");
		cli.AddParam("value", str::Str(message));
		cli.Query("func=" PRESSURE_ACTION);
	}
private:
	RF24 *m_radio;
	RF24Network *m_network;
	bool m_init;
	bool m_stop;
	bool m_can_call;
	bool m_send_time;
	mgr_thread::Handle m_thread;



	void RealSendTime() {
		STrace();
		mgr_date::DateTime now;
		RF24NetworkHeader header(RADIO_REC_MASTER, 'T');

		uint16_t curr_time[7] = {
			(uint16_t) now.year()
			, (uint16_t) now.month()
			, (uint16_t) now.day()
			, (uint16_t) now.hour()
			, (uint16_t) now.minute()
			, (uint16_t) now.sec()
			, (uint16_t) (now.weekday() + 1)
		};

		m_network->multicast(header, &curr_time, sizeof(curr_time), BROADCAST_LEVEL);
	}
} * radio_proc = nullptr;

class HandlingAction : public Action {
public:
	HandlingAction(const string &action_name) : Action(action_name, MinLevel(lvSuper)) {}

	virtual bool IsModify(const Session &ses) const {
		return false;
	}
private:
	virtual void Execute(Session &ses) const {}
};

class SendTimeAction : public Action {
public:
	SendTimeAction() : Action(SEND_TIME_ACTION, MinLevel(lvSuper)) {}

	virtual bool IsModify(const Session &ses) const {
		return false;
	}

private:
	virtual void Execute(Session &ses) const {
		if (radio_proc == nullptr)
			return;

		radio_proc->SendTime();
	}
};

MODULE_INIT(radio, "rfndb") {
	new HandlingAction(ALARM_ACTION);
	new HandlingAction(BUTTON_ACTION);
	new HandlingAction(DHTDATA_ACTION);
	new HandlingAction(PRESSURE_ACTION);
	radio_proc = isp_api::RegisterComponent(new RadioProcessor);

	new SendTimeAction;
	isp_api::task::Schedule(
			mgr_file::ConcatPath(mgr_file::GetCurrentDir(), "sbin/mgrctl")
				+ " -m " + isp_api::GetMgrName() + " " SEND_TIME_ACTION
			, "5 4 * * *", "RFNmanager task to send time"
	);
}

MODULE_INIT(radio_avail, "radio mgr:services") {
	radio_proc->SetIsCallable();
}
} //end of rfnpriv_radio namespace
