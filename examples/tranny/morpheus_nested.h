// nested classes

#ifndef UCL_EE_MORPHEUS_NESTED_H
#define UCL_EE_MORPHEUS_NESTED_H

#include <sstream>
#include <string>
#include <rofl/common/openflow/openflow_rofl_exceptions.h>
#include <rofl/common/openflow/openflow10.h>
#include <rofl/common/cerror.h>
#include <rofl/common/openflow/cofaction.h>

/**
 * MASTER TODO:
 * secondary messages (either replies or those forwarded) may return errors - session handlers should have error_msg handlers, and morpheus should register a timer after a call to a session method.  If this timer expires (it's set to longer than a reply message timeout) and the session is completed then only then can it be removed.
 * assert that DPE config supports VLAN tagging and stripping.
 * Somewhere the number of bytes of ethernet frame to send for packet-in is set - this must be adjusted for removal of VLAN tag
 * There should be a database of match structs that were received as part of a flow mod, and their translated counter-parts so they could be quickly looked up and swapped back
 * During a packet-in - what to do with the buffer-id? Because we've sent a packet fragment up in the message, but the switch's buffer has a different version of the packet => should be ok, as switch won't be making changes to stored packet and will forward when asked.
 * 
 */


class morpheus;

void dumpBytes (std::ostream & os, uint8_t * bytes, size_t n_bytes);

class morpheus::chandlersession_base {
protected:
morpheus * m_parent;
bool m_completed;
chandlersession_base( morpheus * parent ):m_parent(parent),m_completed(false) {}

public:
virtual std::string asString() { return "**chandlersession_base**"; }
virtual bool isCompleted() { return m_completed; }	// returns true if the session has finished its work and shouldn't be kept alive further
virtual void handle_error (rofl::cofdpt *src, rofl::cofmsg *msg) { std::cout << "** Received error message from dpt " << src->c_str() << " with xid ( " << msg->get_xid() << " ): no handler implemented. Dropping it." << std::endl; delete(msg); }
virtual void handle_error (rofl::cofctl *src, rofl::cofmsg *msg) { std::cout << "** Received error message from ctl " << src->c_str() << " with xid ( " << msg->get_xid() << " ): no handler implemented. Dropping it." << std::endl; delete(msg); }
virtual ~chandlersession_base() { std::cout << __FUNCTION__ << " called. Session was " << (m_completed?"":"NOT ") << "completed." << std::endl; }
void push_features(uint32_t new_capabilities, uint32_t new_actions) { m_parent->set_supported_features( new_capabilities, new_actions ); }
};

// TODO make sure that incoming VLAN is stripped
class morpheus::cflow_mod_session : public morpheus::chandlersession_base {
public:
cflow_mod_session(morpheus * parent, rofl::cofctl * const src, rofl::cofmsg_flow_mod * const msg ):chandlersession_base(parent) {
	std::cout << __PRETTY_FUNCTION__ << " called." << std::endl;
	process_flow_mod(src, msg);
	}
bool process_flow_mod ( rofl::cofctl * const src, rofl::cofmsg_flow_mod * const msg ) {
	if(msg->get_version() != OFP10_VERSION) throw rofl::eBadVersion();
	std::cout << "Incoming msg match: " << msg->get_match().c_str() << " msg actions: " << msg->get_actions().c_str() << std::endl;
	struct ofp10_match * p = msg->get_match().ofpu.ofp10u_match;
dumpBytes( std::cout, (uint8_t *) p, sizeof(struct ofp10_match) );
	rofl::cflowentry entry(OFP10_VERSION);
	entry.set_command(msg->get_command());
	if(msg->get_command() != OFPFC_ADD) {
		std::cout << __FUNCTION__ << ": FLOW_MOD command " << (unsigned)msg->get_command() << " not supported. Dropping message." << std::endl;
		m_completed = true;
		return m_completed;
	}
	entry.set_idle_timeout(msg->get_idle_timeout());
	entry.set_hard_timeout(msg->get_hard_timeout());
	entry.set_cookie(msg->get_cookie());
	entry.set_priority(msg->get_priority());
	entry.set_buffer_id(msg->get_buffer_id());
	entry.set_out_port(msg->get_out_port());	// TODO this will have to be translated if the message is OFPFC_DELETE*
	entry.set_flags(msg->get_flags());
std::cout << "TP" << __LINE__ << std::endl;
	entry.match = msg->get_match();
std::cout << "TP" << __LINE__ << std::endl;
	entry.actions = msg->get_actions();
std::cout << "TP" << __LINE__ << std::endl;
	rofl::cofaclist inlist = msg->get_actions();
	
	bool already_set_vlan = false;
	bool already_did_output = false;
// now translate the action and the match
	try {
		entry.actions = m_parent->get_mapper().action_convertor(inlist);
	} catch (rofl::eBadActionBadArgument &) {
		m_parent->send_error_message( src, msg->get_xid(), OFP10ET_FLOW_MOD_FAILED, OFP10FMFC_UNSUPPORTED, msg->soframe(), msg->framelen() );
		assert(false);
		m_completed = true;
		return m_completed;
	}
	catch(rofl::eBadActionBadOutPort &) {
		m_parent->send_error_message(src, msg->get_xid(), OFP10ET_BAD_ACTION, OFP10BAC_BAD_OUT_PORT, msg->soframe(), msg->framelen() );
		assert(false);
		m_completed = true;
		return m_completed;
	}
	
	rofl::cofmatch newmatch = msg->get_match();
	rofl::cofmatch oldmatch = newmatch;

	//check that VLANs are wildcarded (i.e. not being matched on)
	// TODO we *could* theoretically support incoming VLAN iff they are coming in on an port-translated-only port (i.e. a virtual port that doesn't map to a port+vlan, only a phsyical port), and that VLAN si then stripped in the action.
	if(oldmatch.get_vlan_vid_mask() != 0xffff) {
		std::cout << __FUNCTION__ << ": received a match which didn't have VLAN wildcarded. Sending error and dropping message. match:" << oldmatch.c_str() << std::endl;
		m_parent->send_error_message( src, msg->get_xid(), OFP10ET_FLOW_MOD_FAILED, OFP10FMFC_UNSUPPORTED, msg->soframe(), msg->framelen() );
		m_completed = true;
		return m_completed;
	}
	// make sure this is a valid port
	// TODO check whether port is ANY/ALL
	uint32_t old_inport = oldmatch.get_in_port();
	try {
		cportvlan_mapper::port_spec_t real_port = m_parent->get_mapper().get_actual_port( old_inport ); // could throw std::out_of_range
		if(!real_port.vlanid_is_none()) {
			// vlan is set in actual port - update the match
			newmatch.set_vlan_vid( real_port.vlan );
		}
		// update port
		newmatch.set_in_port( real_port.port );
	} catch (std::out_of_range &) {
		std::cout << __FUNCTION__ << ": received a match request for an unknown port (" << old_inport << "). There are " << m_parent->get_mapper().get_number_virtual_ports() << " ports.  Sending error and dropping message. match:" << oldmatch.c_str() << std::endl;
		m_parent->send_error_message( src, msg->get_xid(), OFP10ET_FLOW_MOD_FAILED, OFP10FMFC_UNSUPPORTED, msg->soframe(), msg->framelen() );
		m_completed = true;
		return m_completed;
	}
	
	entry.match = newmatch;
	
	std::cout << __FUNCTION__ << ": About to send flow_mod {";
	std::cout << " command: " << (unsigned) entry.get_command();
	std::cout << ", idle_timeout: " << (unsigned) entry.get_idle_timeout();
	std::cout << ", hard_timeout: " <<  (unsigned) entry.get_hard_timeout();
	std::cout << ", cookie: " << (unsigned) entry.get_cookie();
	std::cout << ", priority: " << (unsigned) entry.get_priority();
	std::cout << ", buffer_id: " << (unsigned) entry.get_buffer_id();
	std::cout << ", out_port: " << (unsigned) entry.get_out_port();
	std::cout << ", match: " << entry.match.c_str();
	std::cout << ", actions: " << entry.actions.c_str() << " }" << std::endl;
	m_parent->send_flow_mod_message( m_parent->get_dpt(), entry );
	m_completed = true;
	return m_completed;
}
~cflow_mod_session() { std::cout << __FUNCTION__ << " called." << std::endl; }	// nothing to do as we didn't register anywhere.
};


class morpheus::cfeatures_request_session : public morpheus::chandlersession_base {
protected:
uint32_t m_request_xid;
bool m_local_request;	// if true then this request originated from morpheus and not as the result of a translated request from a controller
public:
cfeatures_request_session(morpheus * parent):chandlersession_base(parent),m_local_request(true) {
	std::cout << __PRETTY_FUNCTION__ << " called." << std::endl;
	uint32_t newxid = m_parent->send_features_request( m_parent->get_dpt() );
	if( ! m_parent->associate_xid( false, newxid, this ) ) std::cout << "Problem associating dpt xid in " << __FUNCTION__ << std::endl;
	m_completed = false;
	}
cfeatures_request_session(morpheus * parent, const rofl::cofctl * const src, const rofl::cofmsg_features_request * const msg):chandlersession_base(parent),m_local_request(false) {
	std::cout << __PRETTY_FUNCTION__ << " called." << std::endl;
	process_features_request(src, msg);
	}
bool process_features_request ( const rofl::cofctl * const src, const rofl::cofmsg_features_request * const msg ) {
//	if(msg->get_version() != OFP10_VERSION) { std::stringstream ss; ss << "Bad OF version packet received in " << __FUNCTION__; throw rofl::eBadVersion( ss.str() ); }
	if(msg->get_version() != OFP10_VERSION) throw rofl::eBadVersion();
	m_request_xid = msg->get_xid();
	uint32_t newxid = m_parent->send_features_request( m_parent->get_dpt() );
	if( ! m_parent->associate_xid( true, m_request_xid, this ) ) std::cout << "Problem associating ctl xid in " << __FUNCTION__ << std::endl;
	if( ! m_parent->associate_xid( false, newxid, this ) ) std::cout << "Problem associating dpt xid in " << __FUNCTION__ << std::endl;
	m_completed = false;
	return m_completed;
}
bool process_features_reply ( const rofl::cofdpt * const src, rofl::cofmsg_features_reply * const msg ) {
	assert(!m_completed);
	if(msg->get_version() != OFP10_VERSION) throw rofl::eBadVersion();
	if(m_local_request) {
		push_features( msg->get_capabilities(), msg->get_actions_bitmap() );
		m_completed = true;
	} else {
		uint64_t dpid = m_parent->get_dpid() + 1;		// TODO get this from config file
		uint32_t capabilities = msg->get_capabilities();
		// first check whether we have the ones we need, then rewrite them anyway
		std::cout << __FUNCTION__ << ": capabilities of DPE reported as:" << capabilities_to_string(capabilities) << std::endl;
		uint32_t of10_actions_bitmap = msg->get_actions_bitmap();	// 	TODO ofp10 only
		std::cout << __FUNCTION__ << ": supported actions of DPE reported as:" << action_mask_to_string(of10_actions_bitmap) << std::endl;
		push_features( capabilities, of10_actions_bitmap );

		capabilities = m_parent->get_supported_features();	// hard coded to return 0 - No stats, STP or the other magic.
				
		rofl::cofportlist realportlist = msg->get_ports();
		// check whether all the ports we're using are actually supported by the DPE
		
		rofl::cofportlist virtualportlist;

		// TODO this is hardcoded for testing. Need to implement better support for config in morpheus, then just grabbed the translated and validated (based on underlying switch) config from there
		const cportvlan_mapper & mapper = m_parent->get_mapper();
		for(unsigned portno = 1; portno <= mapper.get_number_virtual_ports(); ++portno) {
			cportvlan_mapper::port_spec_t vport = mapper.get_actual_port(portno);
			rofl::cofport p(OFP10_VERSION);
			p.set_config(OFP10PC_NO_STP);
			uint32_t feats = OFP10PF_10GB_FD | OFP10PF_FIBER;
			p.set_peer (feats);
			p.set_curr (feats);
			p.set_advertised (feats);
			p.set_supported (feats);
			p.set_state(0);	// link is up and ignoring STP
			p.set_port_no(portno);
			p.set_hwaddr(rofl::cmacaddr("00:B1:6B:00:B1:E5"));
			std::stringstream ss;
			ss << "V_" << vport;
			p.set_name(std::string(ss.str()));
			virtualportlist.next() = p;
			}
		m_parent->send_features_reply(m_parent->get_ctl(), m_request_xid, dpid, msg->get_n_buffers(), msg->get_n_tables(), capabilities, 0, of10_actions_bitmap, virtualportlist );	// TODO get_action_bitmap is OF1.0 only
		m_completed = true;
	}
	return m_completed;
}
~cfeatures_request_session() { std::cout << __FUNCTION__ << " called." << std::endl; }
std::string asString() { std::stringstream ss; ss << "cfeatures_request_session {request_xid=" << m_request_xid << "}"; return ss.str(); }
};

class morpheus::cget_config_session : public morpheus::chandlersession_base {
protected:
uint32_t m_request_xid;
public:
cget_config_session(morpheus * parent, const rofl::cofctl * const src, const rofl::cofmsg_get_config_request * const msg):chandlersession_base(parent) {
	std::cout << __PRETTY_FUNCTION__ << " called." << std::endl;
	process_config_request(src, msg);
	}
bool process_config_request ( const rofl::cofctl * const src, const rofl::cofmsg_get_config_request * const msg ) {
	if(msg->get_version() != OFP10_VERSION) throw rofl::eBadVersion();
	m_request_xid = msg->get_xid();
	uint32_t newxid = m_parent->send_get_config_request( m_parent->get_dpt() );
	if( ! m_parent->associate_xid( true, m_request_xid, this ) ) std::cout << "Problem associating ctl xid in " << __FUNCTION__ << std::endl;
	if( ! m_parent->associate_xid( false, newxid, this ) ) std::cout << "Problem associating dpt xid in " << __FUNCTION__ << std::endl;
	m_completed = false;
	return m_completed;
}
bool process_config_reply ( const rofl::cofdpt * const src, rofl::cofmsg_get_config_reply * const msg ) {
	assert(!m_completed);
	if(msg->get_version() != OFP10_VERSION) throw rofl::eBadVersion();
	m_parent->send_get_config_reply(m_parent->get_ctl(), m_request_xid, msg->get_flags(), msg->get_miss_send_len() );
	m_completed = true;
	return m_completed;
}
~cget_config_session() { std::cout << __FUNCTION__ << " called." << std::endl; }
std::string asString() { std::stringstream ss; ss << "cget_config_session {request_xid=" << m_request_xid << "}"; return ss.str(); }
};

class morpheus::cdesc_stats_session : public morpheus::chandlersession_base {
protected:
uint32_t m_request_xid;
public:
cdesc_stats_session(morpheus * parent, const rofl::cofctl * const src, const rofl::cofmsg_desc_stats_request * const msg):chandlersession_base(parent) {
	std::cout << __PRETTY_FUNCTION__ << " called." << std::endl;
	process_desc_stats_request(src, msg);
	}
bool process_desc_stats_request ( const rofl::cofctl * const src, const rofl::cofmsg_desc_stats_request * const msg ) {
	if(msg->get_version() != OFP10_VERSION) throw rofl::eBadVersion();
	m_request_xid = msg->get_xid();
	uint32_t newxid = m_parent->send_desc_stats_request(m_parent->get_dpt(), msg->get_stats_flags());
	if( ! m_parent->associate_xid( true, m_request_xid, this ) ) std::cout << "Problem associating ctl xid in " << __FUNCTION__ << std::endl;
	if( ! m_parent->associate_xid( false, newxid, this ) ) std::cout << "Problem associating dpt xid in " << __FUNCTION__ << std::endl;
	m_completed = false;
	return m_completed;
}
bool process_desc_stats_reply ( rofl::cofdpt * const src, rofl::cofmsg_desc_stats_reply * const msg ) {
	assert(!m_completed);
	if(msg->get_version() != OFP10_VERSION) throw rofl::eBadVersion();
	rofl::cofdesc_stats_reply reply(src->get_version(),"morpheus_mfr_desc","morpheus_hw_desc","morpheus_sw_desc","morpheus_serial_num","morpheus_dp_desc");
	m_parent->send_desc_stats_reply(m_parent->get_ctl(), m_request_xid, reply, false );
	m_completed = true;
	return m_completed;
}
~cdesc_stats_session() { std::cout << __FUNCTION__ << " called." << std::endl; }
std::string asString() { std::stringstream ss; ss << "cdesc_stats_session {request_xid=" << m_request_xid << "}"; return ss.str(); }
};

// TODO translation check
class morpheus::ctable_stats_session : public morpheus::chandlersession_base {
protected:
uint32_t m_request_xid;
public:
ctable_stats_session(morpheus * parent, const rofl::cofctl * const src, const rofl::cofmsg_table_stats_request * const msg):chandlersession_base(parent) {
	std::cout << __PRETTY_FUNCTION__ << " called." << std::endl;
	process_table_stats_request(src, msg);
	}
bool process_table_stats_request ( const rofl::cofctl * const src, const rofl::cofmsg_table_stats_request * const msg ) {
	if(msg->get_version() != OFP10_VERSION) throw rofl::eBadVersion();
	m_request_xid = msg->get_xid();
	uint32_t newxid = m_parent->send_table_stats_request(m_parent->get_dpt(), msg->get_stats_flags());
	if( ! m_parent->associate_xid( true, m_request_xid, this ) ) std::cout << "Problem associating ctl xid in " << __FUNCTION__ << std::endl;
	if( ! m_parent->associate_xid( false, newxid, this ) ) std::cout << "Problem associating dpt xid in " << __FUNCTION__ << std::endl;
	m_completed = false;
	return m_completed;
}
bool process_table_stats_reply ( rofl::cofdpt * const src, rofl::cofmsg_table_stats_reply * const msg ) {
	assert(!m_completed);
	if(msg->get_version() != OFP10_VERSION) throw rofl::eBadVersion();
	m_parent->send_table_stats_reply(m_parent->get_ctl(), m_request_xid, msg->get_table_stats(), false ); // TODO how to deal with "more" flag (last arg)
	m_completed = true;
	return m_completed;
}
~ctable_stats_session() { std::cout << __FUNCTION__ << " called." << std::endl; }
std::string asString() { std::stringstream ss; ss << "ctable_stats_session {request_xid=" << m_request_xid << "}"; return ss.str(); }
};

class morpheus::cset_config_session : public morpheus::chandlersession_base {
public:
cset_config_session(morpheus * parent, const rofl::cofctl * const src, const rofl::cofmsg_set_config * const msg ):chandlersession_base(parent) {
	std::cout << __PRETTY_FUNCTION__ << " called." << std::endl;
	process_set_config(src, msg);
	}
bool process_set_config ( const rofl::cofctl * const src, const rofl::cofmsg_set_config * const msg ) {
	if(msg->get_version() != OFP10_VERSION) throw rofl::eBadVersion();
	m_parent->send_set_config_message(m_parent->get_dpt(), msg->get_flags(), msg->get_miss_send_len());
	m_completed = true;
	return m_completed;
}
~cset_config_session() { std::cout << __FUNCTION__ << " called." << std::endl; }	// nothing to do as we didn't register anywhere.
};

// TODO translation check
class morpheus::caggregate_stats_session : public morpheus::chandlersession_base {
protected:
uint32_t m_request_xid;
public:
caggregate_stats_session(morpheus * parent, const rofl::cofctl * const src, rofl::cofmsg_aggr_stats_request * const msg):chandlersession_base(parent) {
	std::cout << __PRETTY_FUNCTION__ << " called." << std::endl;
	process_aggr_stats_request(src, msg);
	}
bool process_aggr_stats_request ( const rofl::cofctl * const src, rofl::cofmsg_aggr_stats_request * const msg ) {
	if(msg->get_version() != OFP10_VERSION) throw rofl::eBadVersion();
	m_request_xid = msg->get_xid();
	uint16_t stats_flags = msg->get_stats_flags();
	rofl::cofaggr_stats_request aggr_req( msg->get_aggr_stats() );
	uint32_t newxid = m_parent->send_aggr_stats_request(m_parent->get_dpt(), stats_flags, aggr_req);
	if( ! m_parent->associate_xid( true, m_request_xid, this ) ) std::cout << "Problem associating ctl xid in " << __FUNCTION__ << std::endl;
	if( ! m_parent->associate_xid( false, newxid, this ) ) std::cout << "Problem associating dpt xid in " << __FUNCTION__ << std::endl;
	m_completed = false;
	return m_completed;
}
bool process_aggr_stats_reply ( rofl::cofdpt * const src, rofl::cofmsg_aggr_stats_reply * const msg ) {
	assert(!m_completed);
	if(msg->get_version() != OFP10_VERSION) throw rofl::eBadVersion();
	m_parent->send_aggr_stats_reply(m_parent->get_ctl(), m_request_xid, msg->get_aggr_stats(), false ); // TODO how to deal with "more" flag (last arg)
	m_completed = true;
	return m_completed;
}
~caggregate_stats_session() { std::cout << __FUNCTION__ << " called." << std::endl; }
std::string asString() { std::stringstream ss; ss << "caggregate_stats_session {request_xid=" << m_request_xid << "}"; return ss.str(); }
};

class morpheus::cpacket_in_session : public morpheus::chandlersession_base {
public:
cpacket_in_session(morpheus * parent, const rofl::cofdpt * const src, rofl::cofmsg_packet_in * const msg ):chandlersession_base(parent) {
	std::cout << __PRETTY_FUNCTION__ << " called." << std::endl;
	process_packet_in(src, msg);
	}
bool process_packet_in ( const rofl::cofdpt * const src, rofl::cofmsg_packet_in * const msg ) {
	if(msg->get_version() != OFP10_VERSION) throw rofl::eBadVersion();
	rofl::cofctl * master = m_parent->get_ctl();
// std::cout << "TP" << __LINE__ << std::endl;
	std::cout << "** BEFORE:" << std::endl;
	rofl::cofmatch match(msg->get_match_const());
std::cout << "TP" << __LINE__ << "match found to be " << match.c_str() << std::endl;	
	rofl::cpacket packet(msg->get_packet_const());
// std::cout << "TP" << __LINE__ << std::endl;
// std::cout << "packet.framelen = " << (unsigned)packet.framelen() << "packet.soframe = " << packet.soframe() << std::endl;
// std::cout << "TP" << __LINE__ << std::endl;
///	packet.get_match().set_in_port(msg->get_in_port());	// JSP: this is unnecessary as packet.get_match is locally generated anyway.
 std::cout << "TP" << __LINE__ << std::endl;
std::cout << "original packet bytes: ";
// dumpBytes( std::cout, msg->get_packet_const().soframe(), msg->get_packet_const().framelen());
dumpBytes( std::cout, packet.soframe(), packet.framelen());
std::cout << std::endl;
std::cout << "frame bytes: ";
//dumpBytes( std::cout, msg->get_packet().frame()->soframe(), msg->get_packet().frame()->framelen());
dumpBytes( std::cout, packet.frame()->soframe(), packet.frame()->framelen());
std::cout << std::endl;
std::cout << "TP" << __LINE__ << std::endl;
std::cout << "source MAC: " << packet.ether()->get_dl_src() << std::endl;
std::cout << "dest MAC: " << packet.ether()->get_dl_dst() << std::endl;
std::cout << "OFP10_PACKET_IN_STATIC_HDR_LEN is " << OFP10_PACKET_IN_STATIC_HDR_LEN << std::endl;
std::cout << "TP" << __LINE__ << std::endl;
std::cout << "** AFTER:" << std::endl;

// extract the VLAN from the incoming packet
int32_t in_port = -1;
int32_t in_vlan = -1;
if(packet.cnt_vlan_tags()>0) {	// check to see if we have a vlan header
	in_vlan = packet.vlan(0)->get_dl_vlan_id();
	if(in_vlan == 0xffff) in_vlan = -1;	// it would be weird for this to happen - there's a vlan frame, but no tag. not sure if it's possible.
}
in_port = msg->get_in_port();

const cportvlan_mapper & mapper = m_parent->get_mapper();

cportvlan_mapper::port_spec_t inport_spec( PV_PORT_T(in_port), (in_vlan==-1)?(PV_VLANID_T::NONE):(PV_VLANID_T(in_vlan)) );

std::vector<std::pair<uint16_t, cportvlan_mapper::port_spec_t> > vports = mapper.actual_to_virtual_map( inport_spec );

std::cout << __FUNCTION__ << ": received incoming packet on port " << in_port << " and vlan " << in_vlan << " which turned into the spec (" << inport_spec << ") which in turn mapped to " << vports.size() << " virtual ports:";
for(std::vector<std::pair<uint16_t, cportvlan_mapper::port_spec_t> >::const_iterator ci=vports.begin(); ci != vports.end(); ++ci) std::cout << " " << (unsigned)ci->first;
std::cout << std::endl;

if(vports.size() != 1) {	// TODO handle this better
	std::cout << __FUNCTION__ << ": Incoming packet on port spec which doesn't match a virtual port. Dropping." << std::endl;
	m_completed = true;
	return m_completed;
}

std::pair<uint16_t, cportvlan_mapper::port_spec_t> vport = vports.front();

// required changes:
// Strip VLAN on incoming packet
// rewrite get_in_port
// fiddle match struct

if(in_vlan != -1) packet.pop_vlan();	// remove the first VLAN header

// what to do with match??
// ANSWER - NOTHING. It's not used in OF10..

	m_parent->send_packet_in_message(master, msg->get_buffer_id(), msg->get_total_len(), msg->get_reason(), 0, 0, vport.first, match, packet.soframe(), packet.framelen() );	// TODO - the length fields are guesses.
//	m_parent->send_packet_in_message(master, msg->get_buffer_id(), msg->get_total_len(), msg->get_reason(), 0, 0, msg->get_in_port(), match, packet.ether()->sopdu(), packet.framelen() );	// TODO - the length fields are guesses.
	std::cout << __FUNCTION__ << " : packet_in forwarded to " << master->c_str() << "." << std::endl;
	m_completed = true;
	return m_completed;
}
~cpacket_in_session() { std::cout << __FUNCTION__ << " called." << std::endl; }	// nothing to do as we didn't register anywhere.
};

// TODO translation check
class morpheus::cpacket_out_session : public morpheus::chandlersession_base {
public:
cpacket_out_session(morpheus * parent, const rofl::cofctl * const src, rofl::cofmsg_packet_out * const msg ):chandlersession_base(parent) {
	std::cout << __PRETTY_FUNCTION__ << " called." << std::endl;
	process_packet_out(src, msg);
	}
bool process_packet_out ( const rofl::cofctl * const src, rofl::cofmsg_packet_out * const msg ) {
	if(msg->get_version() != OFP10_VERSION) throw rofl::eBadVersion();
	if(msg->get_buffer_id()!=-1) {
		std::cout << __FUNCTION__ << ": buffered packets in PacketOut not supported." << std::endl;
		assert(false);
		m_completed = true;
		return m_completed;
	}
	rofl::cofaclist actions(msg->get_actions());	// These actions are to be performed on either the buffered packet, or the one included in this message.
	std::cout << "*** Actions copied: ";
	for(rofl::cofaclist::iterator i = actions.begin(); i != actions.end(); ++i)
		std::cout << i->c_str() << " ";
	std::cout << std::endl;
	std::cout << "TP" << __LINE__ << std::endl;
	rofl::cpacket packet(msg->get_packet());
	std::cout << "TP" << __LINE__ << std::endl;

	if(packet.cnt_vlan_tags()>0) {
		std::cout << __FUNCTION__ << ": vlan tags in PacketOut packets not supported." << std::endl;
		assert(false);
		m_completed = true;
		return m_completed;
	}

	const cportvlan_mapper & mapper = m_parent->get_mapper();
	
	uint16_t in_vport = msg->get_in_port();
	std::cout << "packet_out in port is " <<  port_as_string(in_vport) << std::endl;
	
	for(rofl::cofaclist::iterator i = actions.begin(); i != actions.end(); ++i) {
		// cannot support 
		std::cout << i->c_str() << " ";
	}
	switch(in_vport) {
		case OFPP10_ALL: 	// All physical ports except input port. */

		case OFPP10_IN_PORT:// send the packet out the input port.  This virtual port must be explicitly used in order to send back out of the input port.
		case OFPP10_TABLE:	// Perform actions in flow table. NB: This can only be the destination port for packet-out messages.
		case OFPP10_NORMAL:	// Process with normal L2/L3 switching.
		case OFPP10_CONTROLLER:	// Send to controller.
		case OFPP10_LOCAL:	// Local OpenFlow "port".
		case OFPP10_NONE:	// Not associated with a physical port.
		case OFPP10_FLOOD: 	// All physical ports except input port and those disabled by STP.
			std::cout << __FUNCTION__ << " doesn't support output port " << std::hex << (unsigned) in_vport << std::endl;
			m_completed = true;
			return m_completed;
		default:	// an actual numbered port
//		if(in_vport > OFPP10_MAX ) panic
			break;
	};
	cportvlan_mapper::port_spec_t inport_spec( mapper.get_actual_port(msg->get_in_port()) );
	if(!inport_spec.vlanid_is_none()) {
		packet.push_vlan(rofl::fvlanframe::VLAN_CTAG_ETHER);	// create new vlan header
		packet.vlan()->set_dl_vlan_id(inport_spec.vlan);				// set the VLAN ID
	}
	m_parent->send_packet_out_message(m_parent->get_dpt(), msg->get_buffer_id(), inport_spec.port, actions, packet.soframe(), packet.framelen() );	// TODO - the length fields are guesses.

	m_completed = true;
	return m_completed;
}
~cpacket_out_session() { std::cout << __FUNCTION__ << " called." << std::endl; }	// nothing to do as we didn't register anywhere.

};

class morpheus::cbarrier_session : public morpheus::chandlersession_base {
protected:
uint32_t m_request_xid;
public:
cbarrier_session(morpheus * parent, const rofl::cofctl * const src, const rofl::cofmsg_barrier_request * const msg):chandlersession_base(parent) {
	std::cout << __PRETTY_FUNCTION__ << " called." << std::endl;
	process_barrier_request(src, msg);
	}
bool process_barrier_request ( const rofl::cofctl * const src, const rofl::cofmsg_barrier_request * const msg ) {
	if(msg->get_version() != OFP10_VERSION) throw rofl::eBadVersion();
	m_request_xid = msg->get_xid();
	uint32_t newxid = m_parent->send_barrier_request( m_parent->get_dpt() );
	if( ! m_parent->associate_xid( true, m_request_xid, this ) ) std::cout << "Problem associating ctl xid in " << __FUNCTION__ << std::endl;
	if( ! m_parent->associate_xid( false, newxid, this ) ) std::cout << "Problem associating dpt xid in " << __FUNCTION__ << std::endl;
	m_completed = false;
	return m_completed;
}
bool process_barrier_reply ( const rofl::cofdpt * const src, rofl::cofmsg_barrier_reply * const msg ) {
	assert(!m_completed);
	if(msg->get_version() != OFP10_VERSION) throw rofl::eBadVersion();
	m_parent->send_barrier_reply(m_parent->get_ctl(), m_request_xid );
	m_completed = true;
	return m_completed;
}
~cbarrier_session() { std::cout << __FUNCTION__ << " called." << std::endl; }
std::string asString() { std::stringstream ss; ss << "cbarrier_session {request_xid=" << m_request_xid << "}"; return ss.str(); }
};

// TODO translation check
class morpheus::ctable_mod_session : public morpheus::chandlersession_base {
public:
ctable_mod_session(morpheus * parent, rofl::cofctl * const src, rofl::cofmsg_table_mod * const msg ):chandlersession_base(parent) {
	std::cout << __PRETTY_FUNCTION__ << " called." << std::endl;
	process_table_mod(src, msg);
	}
bool process_table_mod ( rofl::cofctl * const src, rofl::cofmsg_table_mod * const msg ) {
	if(msg->get_version() != OFP10_VERSION) throw rofl::eBadVersion();
	m_parent->send_table_mod_message( m_parent->get_dpt(), msg->get_table_id(), msg->get_config() );
	m_completed = true;
	return m_completed;
}
~ctable_mod_session() { std::cout << __FUNCTION__ << " called." << std::endl; }	// nothing to do as we didn't register anywhere.
};

// TODO translation check
class morpheus::cport_mod_session : public morpheus::chandlersession_base {
public:
cport_mod_session(morpheus * parent, rofl::cofctl * const src, rofl::cofmsg_port_mod * const msg ):chandlersession_base(parent) {
	std::cout << __PRETTY_FUNCTION__ << " called." << std::endl;
	process_port_mod(src, msg);
	}
bool process_port_mod ( rofl::cofctl * const src, rofl::cofmsg_port_mod * const msg ) {
	if(msg->get_version() != OFP10_VERSION) throw rofl::eBadVersion();
	m_parent->send_port_mod_message( m_parent->get_dpt(), msg->get_port_no(), msg->get_hwaddr(), msg->get_config(), msg->get_mask(), msg->get_advertise() );
	m_completed = true;
	return m_completed;
}
~cport_mod_session() { std::cout << __FUNCTION__ << " called." << std::endl; }	// nothing to do as we didn't register anywhere.
};

/*
class morpheus::cqueue_stats_session : public morpheus::chandlersession_base {
// cqueue_stats_session(morpheus * parent, rofl::cxidowner * originator, rofl::cofmsg * msg):chandlersession_base(parent,originator,msg) { process(originator, msg); }
};

class morpheus::cport_stats_session : public morpheus::chandlersession_base {
// cport_stats_session(morpheus * parent, rofl::cxidowner * originator, rofl::cofmsg * msg):chandlersession_base(parent,originator,msg) { process(originator, msg); }
};

class morpheus::cflow_stats_session : public morpheus::chandlersession_base {
// cflow_stats_session(morpheus * parent, rofl::cxidowner * originator, rofl::cofmsg * msg):chandlersession_base(parent,originator,msg) { process(originator, msg); }
};



*/

#endif // UCL_EE_MORPHEUS_NESTED_H
