/*
 * crofconn.cc
 *
 *  Created on: 31.12.2013
 *      Author: andreas
 */

#include "crofconn.h"

using namespace rofl::openflow;

crofconn::crofconn(
		crofconn_env *env,
		uint8_t auxiliary_id,
		int sd,
		caddress const& ra) :
				env(env),
				auxiliary_id(auxiliary_id),
				rofsock(new crofsock(this, sd, ra)),
				ofp_version(OFP_VERSION_UNKNOWN),
				state(STATE_DISCONNECTED),
				hello_timeout(DEFAULT_HELLO_TIMEOUT),
				echo_timeout(DEFAULT_ECHO_TIMEOUT),
				echo_interval(DEFAULT_ECHO_INTERVAL)
{
	run_engine(EVENT_CONNECTED); // socket is available
}



crofconn::~crofconn()
{

}



void
crofconn::handle_timeout(
		int opaque)
{
	switch (opaque) {
	case TIMER_WAIT_FOR_HELLO: {
		events.push_back(EVENT_HELLO_EXPIRED);
	} break;
	case TIMER_SEND_ECHO: {
		action_send_echo_request();
	} break;
	case TIMER_WAIT_FOR_ECHO: {
		events.push_back(EVENT_ECHO_EXPIRED);
	} break;
	default: {
		logging::warn << "[rofl][conn] unknown timer type:" << opaque << " rcvd" << std::endl;
	};
	}
	run_engine();
}



void
crofconn::run_engine(crofconn_event_t event)
{
	if (EVENT_NONE != event) {
		events.push_back(event);
	}

	while (not events.empty()) {
		enum crofconn_event_t event = events.front();
		events.pop_front();

		switch (event) {
		case EVENT_CONNECTED: 		event_connected(); 			break;
		case EVENT_DISCONNECTED:	event_disconnected();		break;
		case EVENT_HELLO_RCVD:		event_hello_rcvd();			break;
		case EVENT_HELLO_EXPIRED:	event_hello_expired();		break;
		case EVENT_ECHO_EXPIRED:	event_echo_expired();		break;
		default: {
			logging::error << "[rofl][conn] unknown event seen, internal error" << std::endl << *this;
		};
		}
	}
}



void
crofconn::event_connected()
{
	switch (state) {
	case STATE_DISCONNECTED:
	case STATE_WAIT_FOR_HELLO:
	case STATE_ESTABLISHED: {
		action_send_hello_message();
		reset_timer(TIMER_WAIT_FOR_HELLO, hello_timeout);
		state = STATE_WAIT_FOR_HELLO;

	} break;
	default: {
		logging::error << "[rofl][conn] event -CONNECTED- invalid state reached, internal error" << std::endl << *this;
	};
	}
}



void
crofconn::event_disconnected()
{
	switch (state) {
	case STATE_DISCONNECTED: {
		// do nothing
	} break;
	case STATE_WAIT_FOR_HELLO:
	case STATE_ESTABLISHED: {
		cancel_timer(TIMER_WAIT_FOR_ECHO);
		cancel_timer(TIMER_WAIT_FOR_HELLO);
		rofsock->get_socket().cclose();
		env->handle_close(this);
		state = STATE_DISCONNECTED;

	} break;
	default: {
		logging::error << "[rofl][conn] event -DISCONNECTED- invalid state reached, internal error" << std::endl << *this;
	};
	}
}



void
crofconn::event_hello_rcvd()
{
	switch (state) {
	case STATE_DISCONNECTED: {
		action_send_hello_message();
		reset_timer(TIMER_WAIT_FOR_HELLO, hello_timeout);
		state = STATE_WAIT_FOR_HELLO;

	} break;
	case STATE_WAIT_FOR_HELLO:
	case STATE_ESTABLISHED: {
		cancel_timer(TIMER_WAIT_FOR_HELLO);
		reset_timer(TIMER_SEND_ECHO, echo_interval);
		state = STATE_ESTABLISHED;

	} break;
	default: {
		logging::error << "[rofl][conn] event -HELLO-RCVD- occured in invalid state, internal error" << std::endl << *this;
	};
	}
}



void
crofconn::event_hello_expired()
{
	switch (state) {
	case STATE_WAIT_FOR_HELLO: {
		logging::warn << "[rofl][conn] event -HELLO-EXPIRED- occured in state -WAIT-FOR-HELLO-" << std::endl << *this;
		run_engine(EVENT_DISCONNECTED);

	} break;
	default: {
		logging::error << "[rofl][conn] event -HELLO-EXPIRED- occured in invalid state, internal error" << std::endl << *this;
	};
	}
}



void
crofconn::event_echo_rcvd()
{
	switch (state) {
	case STATE_ESTABLISHED: {
		cancel_timer(TIMER_WAIT_FOR_ECHO);
		register_timer(TIMER_SEND_ECHO, echo_interval);

	} break;
	default: {
		logging::error << "[rofl][conn] event -ECHO.reply-RCVD- occured in invalid state, internal error" << std::endl << *this;
	};
	}
}



void
crofconn::event_echo_expired()
{
	switch (state) {
	case STATE_ESTABLISHED: {
		run_engine(EVENT_DISCONNECTED);

	} break;
	default: {
		logging::error << "[rofl][conn] event -ECHO.reply-EXPIRED- occured in invalid state, internal error" << std::endl << *this;
	};
	}
}



void
crofconn::action_send_hello_message()
{
	try {
		if (versionbitmap.get_highest_ofp_version() == OFP_VERSION_UNKNOWN) {
			logging::warn << "[rofl][conn] unable to send HELLO message, as no OFP versions are currently configured" << std::endl;
			return;
		}

		cmemory body(versionbitmap.length());
		versionbitmap.pack(body.somem(), body.memlen());

		cofmsg_hello *hello =
				new cofmsg_hello(
						versionbitmap.get_highest_ofp_version(),
						env->get_async_xid(this),
						body.somem(), body.memlen());

		rofsock->send_message(hello);

	} catch (eRofConnXidSpaceExhausted& e) {

		logging::error << "[rofl][conn] sending HELLO.message failed: no idle xid available" << std::endl << *this;

	} catch (RoflException& e) {

		logging::error << "[rofl][conn] sending HELLO.message failed " << std::endl << *this;

	}
}



void
crofconn::action_send_echo_request()
{
	try {
		cofmsg_echo_request *echo =
				new cofmsg_echo_request(
						ofp_version,
						env->get_sync_xid(this));

		rofsock->send_message(echo);

	} catch (eRofConnXidSpaceExhausted& e) {

		logging::error << "[rofl][conn] sending ECHO.request failed: no idle xid available" << std::endl << *this;


	} catch (RoflException& e) {

		logging::error << "[rofl][conn] sending ECHO.request failed " << *this << std::endl;

	}
}



void
crofconn::handle_connect_refused(crofsock *endpnt)
{

}



void
crofconn::handle_open (crofsock *endpnt)
{

}



void
crofconn::handle_close(crofsock *endpnt)
{

}



void
crofconn::recv_message(
		crofsock *endpnt,
		cofmsg *msg)
{
	switch (msg->get_type()) {
	case OFPT_HELLO: 		hello_rcvd(msg);				break;
	case OFPT_ERROR: 		error_rcvd(msg);				break;
	case OFPT_ECHO_REQUEST: echo_request_rcvd(msg);			break;
	case OFPT_ECHO_REPLY: 	echo_reply_rcvd(msg);			break;
	default:				env->recv_message(this, msg);	break;
	}
}



void
crofconn::hello_rcvd(
		cofmsg *msg)
{
	cofmsg_hello *hello = dynamic_cast<cofmsg_hello*>( msg );

	if (NULL == msg) {
		logging::debug << "[rofl][conn] invalid message rcvd in method hello_rcvd()" << std::endl << *msg;
		delete msg; return;
	}

	try {

		/* Step 1: extract version information from HELLO message */

		versionbitmap_peer.clear();

		switch (hello->get_version()) {
		case openflow10::OFP_VERSION:
		case openflow12::OFP_VERSION: {
			versionbitmap_peer.add_ofp_version(hello->get_version());
		} break;
		default: { // msg->get_version() should contain the highest number of supported OFP versions encoded in versionbitmap
			cofhelloelems helloIEs(hello->get_body());
			if (not helloIEs.has_hello_elem_versionbitmap()) {
				logging::warn << "[rofl][conn] HELLO message rcvd without HelloIE -VersionBitmap-" << *hello << std::endl;
				versionbitmap_peer.add_ofp_version(hello->get_version());
			} else {
				versionbitmap_peer = helloIEs.get_hello_elem_versionbitmap();
				// sanity check
				if (not versionbitmap_peer.has_ofp_version(hello->get_version())) {
					logging::warn << "[rofl][conn] malformed HelloIE -VersionBitmap- => " <<
							"does not contain version defined in OFP message header:" <<
							(int)hello->get_version() << std::endl << *hello;
				}
			}
		};
		}

		/* Step 2: select highest supported protocol version on both sides */

		cofhello_elem_versionbitmap versionbitmap_common = versionbitmap & versionbitmap_peer;
		if (versionbitmap_common.get_highest_ofp_version() == OFP_VERSION_UNKNOWN) {
			logging::warn << "[rofl][conn] no common OFP version found for peer" << std::endl;
			logging::warn << "local version-bitmap:" << std::endl << indent(2) << versionbitmap;
			logging::warn << "remote version-bitmap:" << std::endl << indent(2) << versionbitmap_peer;
			throw eHelloIncompatible();
		}

		ofp_version = versionbitmap_common.get_highest_ofp_version();

		logging::info << "[rofl][conn] negotiated OFP version:" << (int)ofp_version << std::endl;

		// move on state machine
		if (ofp_version == OFP_VERSION_UNKNOWN) {
			logging::warn << "[rofl][conn] no common OFP version supported, closing connection." << std::endl << *this;
			run_engine(EVENT_DISCONNECTED);
		} else {
			logging::info << "[rofl][conn] connection established." << std::endl << *this;
			run_engine(EVENT_HELLO_RCVD);
		}

	} catch (eHelloIncompatible& e) {

		logging::warn << "[rofl][conn] eHelloIncompatible " << *msg << std::endl;

		uint16_t type = 0, code = 0;

		switch (hello->get_version()) {
		case openflow10::OFP_VERSION: {
			type = openflow10::OFPET_HELLO_FAILED; code = openflow10::OFPHFC_INCOMPATIBLE;
		} break;
		case openflow12::OFP_VERSION: {
			type = openflow12::OFPET_HELLO_FAILED; code = openflow12::OFPHFC_INCOMPATIBLE;
		} break;
		case openflow13::OFP_VERSION: {
			type = openflow13::OFPET_HELLO_FAILED; code = openflow13::OFPHFC_INCOMPATIBLE;
		} break;
		default: {
			logging::warn << "[rofl][crofbase] cannot send HelloFailed/Incompatible for ofp-version:"
					<< (int)hello->get_version() << std::endl;
		} delete msg; return;
		}

		cofmsg_error *error =
				new cofmsg_error(
						hello->get_version(), hello->get_xid(), type, code,
							msg->soframe(), msg->framelen());
		delete msg;

		rofsock->send_message(error);

		// TODO: close socket
		//rofsock->

		run_engine(EVENT_DISCONNECTED);

	} catch (eHelloEperm& e) {

		logging::warn << "eHelloEperm " << *msg << std::endl;

		uint16_t type = 0, code = 0;

		switch (hello->get_version()) {
		case openflow10::OFP_VERSION: {
			type = openflow10::OFPET_HELLO_FAILED; code = openflow10::OFPHFC_EPERM;
		} break;
		case openflow12::OFP_VERSION: {
			type = openflow12::OFPET_HELLO_FAILED; code = openflow12::OFPHFC_EPERM;
		} break;
		case openflow13::OFP_VERSION: {
			type = openflow13::OFPET_HELLO_FAILED; code = openflow13::OFPHFC_EPERM;
		} break;
		default: {
			logging::warn << "[rofl][crofbase] cannot send HelloFailed/EPerm for ofp-version:"
					<< (int)hello->get_version() << std::endl;
		} delete msg; return;
		}

		cofmsg_error *error =
				new cofmsg_error(
						hello->get_version(), hello->get_xid(), type, code,
							msg->soframe(), msg->framelen());
		delete msg;

		rofsock->send_message(error);

		// TODO: close socket
		// rofsock->

		run_engine(EVENT_DISCONNECTED);

	} catch (RoflException& e) {

		logging::warn << "[rofl][conn] RoflException " << *msg << std::endl;

		delete msg;

		run_engine(EVENT_DISCONNECTED);
	}

}



void
crofconn::echo_request_rcvd(
		cofmsg *msg)
{

}



void
crofconn::echo_reply_rcvd(
		cofmsg *msg)
{

}



void
crofconn::error_rcvd(
		cofmsg *msg)
{

}

