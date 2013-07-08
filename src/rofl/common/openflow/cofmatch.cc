/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "cofmatch.h"

using namespace rofl;

cofmatch::cofmatch(
		uint8_t of_version,
		uint16_t type) :
				of_version(of_version)
{
	switch (of_version) {
	case OFP10_VERSION: {
		memarea.resize(OFP10_MATCH_STATIC_LEN);
		ofh10_match = (struct ofp10_match*)memarea.somem();
	} break;
	case OFP12_VERSION: {
		memarea.resize(OFP12_MATCH_STATIC_LEN);
		ofh12_match = (struct ofp12_match*)memarea.somem();
		ofh12_match->type 	= htobe16(type);
		ofh12_match->length = htobe16(length());
	} break;
	case OFP13_VERSION: {
		memarea.resize(OFP13_MATCH_STATIC_LEN);
		ofh13_match = (struct ofp13_match*)memarea.somem();
		ofh13_match->type 	= htobe16(type);
		ofh13_match->length = htobe16(length());
	} break;
	default:
		throw eBadVersion();
	}
	//WRITELOG(COFMATCH, DBG, "cofmatch(%p)::cofmatch() [1]", this);

	clear();

	validate();
}



template<class T>
cofmatch::cofmatch(
		uint8_t of_version,
		T* match,
		size_t matchlen) :
			of_version(of_version)
{
	//WRITELOG(COFMATCH, DBG, "cofmatch(%p)::cofmatch() [2]", this);

	switch (of_version) {
	case OFP10_VERSION: {
		if (OFP10_MATCH_STATIC_LEN != matchlen) {
			throw eBadVersion();
		}
		unpack(match, matchlen);
	} break;
	case OFP12_VERSION: {
		if (OFP12_MATCH_STATIC_LEN != matchlen) {
			throw eBadVersion();
		}
		unpack(match, matchlen);
	} break;
	case OFP13_VERSION: {
		if (OFP13_MATCH_STATIC_LEN != matchlen) {
			throw eBadVersion();
		}
		unpack(match, matchlen);
	} break;
	default:
		throw eBadVersion();
	}

	validate();
}



cofmatch::~cofmatch()
{
	//WRITELOG(COFMATCH, DBG, "cofmatch(%p)::~cofmatch()", this);
}



cofmatch&
cofmatch::operator= (const cofmatch& m)
{
	if (this == &m)
		return *this;

#if 0
	WRITELOG(COFMATCH, DBG, "cofmatch(%p)::operator=() m:%p", this, &m);

	WRITELOG(COFMATCH, DBG, "cofmatch(%p)::operator=() [1] *this: %s", this, this->c_str());
#endif

	of_version		= m.of_version;
	memarea			= m.memarea;
	oxmlist			= m.oxmlist;

	ofh_match = memarea.somem();

#if 0
	WRITELOG(COFMATCH, DBG, "cofmatch(%p)::operator=() [2] *this: %s", this, this->c_str());
#endif

	validate();

	return *this;
}



bool
cofmatch::operator< (
		cofmatch const& m) const
{
	if (of_version != m.of_version) {
		return (of_version < m.of_version);
	}

	switch (of_version) {
	case OFP10_VERSION: return (memcmp(memarea.somem(), m.memarea.somem(), memarea.memlen()));
	case OFP12_VERSION: return (oxmlist < m.oxmlist);
	case OFP13_VERSION: return (oxmlist < m.oxmlist);
	default: throw eBadVersion();
	}
	return true;
}



void
cofmatch::clear()
{
	//WRITELOG(COFMATCH, DBG, "cofmatch(%p)::reset()", this);

	switch (of_version) {
	case OFP10_VERSION: {
		memset(ofh10_match, 0, OFP10_MATCH_STATIC_LEN);
	} break;
	case OFP12_VERSION: {
		oxmlist.clear();
		ofh12_match->length = htobe16(length());
	} break;
	case OFP13_VERSION: {
		oxmlist.clear();
		ofh13_match->length = htobe16(length());
	} break;
	default:
		throw eBadVersion();
	}
}



bool
cofmatch::is_matching(
		cofmatch& other,
		uint16_t& exact_matches,
		uint16_t& wildcard_matches,
		uint16_t& missed)
{
	WRITELOG(COXMLIST, DBG, "cofmatch(%p)::is_matching()", this);

	exact_matches = 0;
	wildcard_matches = 0;
	missed = 0;

	return oxmlist.is_matching(other.oxmlist, exact_matches, wildcard_matches, missed);
}




void
cofmatch::remove(
		uint16_t oxm_class,
		uint8_t oxm_field)
{
	oxmlist.erase(oxm_class, oxm_field);
}



void
cofmatch::validate() throw (eOFmatchInval)
{
	// TODO: apply OF1.2 prerequisites here
}



size_t
cofmatch::length_internal()
{
	switch (of_version) {
	case OFP10_VERSION: {
		return OFP10_MATCH_STATIC_LEN;
	} break;
	case OFP12_VERSION:
	case OFP13_VERSION: {
		/*
		 * returns length of struct ofp_match including padding
		 */
		size_t match_len = 2 * sizeof(uint16_t); // first two 16bit fields in struct ofp_match

		match_len += oxmlist.length();

		if (0 != (match_len % sizeof(uint64_t)))
		{
			match_len = ((match_len / sizeof(uint64_t)) + 1) * sizeof(uint64_t);
		}
		return match_len;
	} break;
	default:
		throw eBadVersion();
	}
	return 0;
}



size_t
cofmatch::length() const
{
	switch (of_version) {
	case OFP10_VERSION: {
		return OFP10_MATCH_STATIC_LEN;
	} break;
	case OFP12_VERSION:
	case OFP13_VERSION: {
		size_t total_length = 2*sizeof(uint16_t) + oxmlist.length(); // type + length + list

		size_t pad = (0x7 & total_length);
		/* append padding if not a multiple of 8 */
		if (pad) {
			total_length += 8 - pad;
		}
		return total_length;
	}
	default:
		throw eBadVersion();
	}
	return 0;
}



struct ofp10_match*
cofmatch::pack(struct ofp10_match* m, size_t mlen) const throw (eOFmatchInval)
{
	if (mlen < length()) {
		throw eOFmatchInval();
	}

	uint32_t wildcards = 0;

	/*
	 * fill in real length, i.e. excluding padding
	 */
	memset(m, 0, mlen);

	// in_port
	if (oxmlist.exists(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_IN_PORT)) {
		m->in_port = htobe16((uint16_t)(oxmlist.get_oxm(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_IN_PORT).u32value() && 0x0000ffff));
	} else {
		wildcards |= OFP10FW_IN_PORT;
	}

	// dl_src
	if (oxmlist.exists(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_ETH_SRC)) {
		cmacaddr maddr = oxmlist.get_oxm(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_ETH_SRC).u48addr();
		memcpy(m->dl_src, maddr.somem(), OFP_ETH_ALEN);
	} else {
		wildcards |= OFP10FW_DL_SRC;
	}

	// dl_dst
	if (oxmlist.exists(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_ETH_DST)) {
		cmacaddr maddr = oxmlist.get_oxm(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_ETH_DST).u48addr();
		memcpy(m->dl_dst, maddr.somem(), OFP_ETH_ALEN);
	} else {
		wildcards |= OFP10FW_DL_DST;
	}

	// dl_vlan
	if (oxmlist.exists(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_VLAN_VID)) {
		m->dl_vlan = htobe16(oxmlist.get_oxm(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_VLAN_VID).u16value());
	} else {
		wildcards |= OFP10FW_DL_VLAN;
	}

	// dl_vlan_pcp
	if (oxmlist.exists(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_VLAN_PCP)) {
		m->dl_vlan_pcp = oxmlist.get_oxm(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_VLAN_PCP).u8value();
	} else {
		wildcards |= OFP10FW_DL_VLAN_PCP;
	}

	// dl_type
	if (oxmlist.exists(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_ETH_TYPE)) {
		m->dl_type = htobe16(oxmlist.get_oxm(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_ETH_TYPE).u16value());
	} else {
		wildcards |= OFP10FW_DL_TYPE;
	}

	// nw_tos
	if (oxmlist.exists(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_IP_DSCP)) {
		m->nw_tos = oxmlist.get_oxm(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_IP_DSCP).u8value();
	} else {
		wildcards |= OFP10FW_NW_TOS;
	}

	// nw_proto
	if (oxmlist.exists(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_IP_PROTO)) {
		m->nw_tos = oxmlist.get_oxm(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_IP_PROTO).u8value();
	} else {
		wildcards |= OFP10FW_NW_PROTO;
	}

	// nw_src
	if (oxmlist.exists(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_IPV4_SRC)) {
		m->nw_src = htobe32(oxmlist.get_oxm(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_IPV4_SRC).u32value());
		if (oxmlist.get_oxm(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_IPV4_SRC).get_oxm_hasmask()) {
			std::bitset<32> mask(oxmlist.get_oxm(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_IPV4_SRC).uint32_mask());
			wildcards |= ((32 - mask.count()) << OFP10FW_NW_SRC_SHIFT) & OFP10FW_NW_SRC_MASK;
		}
	} else {
		wildcards |= OFP10FW_NW_SRC_ALL;
	}

	// nw_dst
	if (oxmlist.exists(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_IPV4_DST)) {
		m->nw_dst = htobe32(oxmlist.get_oxm(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_IPV4_DST).uint32_value());
		if (oxmlist.get_oxm(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_IPV4_DST).get_oxm_hasmask()) {
			std::bitset<32> mask(oxmlist.get_oxm(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_IPV4_DST).uint32_mask());
			wildcards |= ((32 - mask.count()) << OFP10FW_NW_DST_SHIFT) & OFP10FW_NW_DST_MASK;
		}
	} else {
		wildcards |= OFP10FW_NW_DST_ALL;
	}

	// tp_src
	if (oxmlist.exists(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_TCP_SRC)) {
		m->tp_src = htobe16(oxmlist.get_oxm(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_TCP_SRC).u16value());
	} else {
		wildcards |= OFP10FW_TP_SRC;
	}

	// tp_dst
	if (oxmlist.exists(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_TCP_DST)) {
		m->tp_dst = htobe16(oxmlist.get_oxm(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_TCP_DST).u16value());
	} else {
		wildcards |= OFP10FW_TP_DST;
	}

	m->wildcards = htobe32(wildcards);

	return m;
}



void
cofmatch::unpack(struct ofp10_match* m, size_t mlen)
{
	of_version = OFP10_VERSION;
	oxmlist.clear();
	memarea.resize(OFP10_MATCH_STATIC_LEN);
	ofh10_match = (struct ofp10_match*)memarea.somem();

	if (mlen < OFP10_MATCH_STATIC_LEN) {
		throw eOFmatchInval();
	}

	uint32_t wildcards = be32toh(m->wildcards);

	// in_port
	if (!(wildcards & OFP10FW_IN_PORT)) {
		set_in_port(be32toh(m->in_port));
	}

	// dl_src
	if (!(wildcards & OFP10FW_DL_SRC)) {
		set_eth_src(cmacaddr(m->dl_src, OFP_ETH_ALEN));
	}

	// dl_dst
	if (!(wildcards & OFP10FW_DL_DST)) {
		set_eth_dst(cmacaddr(m->dl_dst, OFP_ETH_ALEN));
	}

	// dl_vlan
	if (!(wildcards & OFP10FW_DL_VLAN)) {
		set_vlan_vid(be16toh(m->dl_vlan));
	}

	// dl_vlan_pcp
	if (!(wildcards & OFP10FW_DL_VLAN_PCP)) {
		set_vlan_pcp(m->dl_vlan_pcp);
	}

	// dl_type
	if (!(wildcards & OFP10FW_DL_TYPE)) {
		set_eth_type(be16toh(m->dl_type));
	}

	// nw_tos
	if (!(wildcards & OFP10FW_NW_TOS)) {
		set_ip_dscp(m->nw_tos);
	}

	// nw_proto
	if (!(wildcards & OFP10FW_NW_PROTO)) {
		set_ip_proto(m->nw_proto);
	}

	// nw_src
	{
		uint32_t num_of_bits = (wildcards & OFP10FW_NW_SRC_MASK) >> OFP10FW_NW_SRC_SHIFT;
		uint32_t u_mask = ~((1 << num_of_bits) - 1);
		caddress addr(AF_INET, "0.0.0.0");
		caddress mask(AF_INET, "0.0.0.0");
		addr.ca_s4addr->sin_addr.s_addr = m->nw_src;
		mask.ca_s4addr->sin_addr.s_addr = htobe32(u_mask);
		if (num_of_bits > 0) {
			set_ipv4_src(addr, mask);
		} else {
			set_ipv4_src(addr);
		}
	}

	// nw_dst
	{
		uint32_t num_of_bits = (wildcards & OFP10FW_NW_DST_MASK) >> OFP10FW_NW_DST_SHIFT;
		uint32_t u_mask = ~((1 << num_of_bits) - 1);
		caddress addr(AF_INET, "0.0.0.0");
		caddress mask(AF_INET, "0.0.0.0");
		addr.ca_s4addr->sin_addr.s_addr = m->nw_dst;
		mask.ca_s4addr->sin_addr.s_addr = htobe32(u_mask);
		if (num_of_bits > 0) {
			set_ipv4_dst(addr, mask);
		} else {
			set_ipv4_dst(addr);
		}
	}

	// tp_src
	if (!(wildcards & OFP10FW_TP_SRC)) {
		set_tcp_src(be16toh(m->tp_src));
	}

	// tp_dst
	if (!(wildcards & OFP10FW_TP_DST)) {
		set_tcp_dst(be16toh(m->tp_dst));
	}

	validate();
}



struct ofp12_match*
cofmatch::pack(struct ofp12_match* m, size_t mlen) const throw (eOFmatchInval)
{
	if (mlen < length()) {
		throw eOFmatchInval();
	}

	/*
	 * fill in real length, i.e. excluding padding
	 */
	ofh12_match->length = htobe16(2 * sizeof(uint16_t) + oxmlist.length());

	m->type 	= ofh12_match->type;
	m->length 	= ofh12_match->length;

	oxmlist.pack((struct ofp_oxm_hdr*)(m->oxm_fields), oxmlist.length());

	return m;
}



void
cofmatch::unpack(struct ofp12_match* m, size_t mlen)
{
	of_version = OFP12_VERSION;
	oxmlist.clear();
	memarea.resize(OFP12_MATCH_STATIC_LEN);
	ofh12_match = (struct ofp12_match*)memarea.somem();
	ofh12_match->type 	= htobe16(OFPMT_OXM);
	ofh12_match->length = htobe16(length());


	if (mlen < (sizeof(uint16_t) + sizeof(uint16_t))) {
		throw eOFmatchInval();
	}

	ofh12_match->type	= (m->type);
	ofh12_match->length	= (m->length);

	if (OFPMT_OXM != be16toh(ofh12_match->type)) {
		throw eBadMatchBadType();
	}

	mlen -= 2 * sizeof(uint16_t);

	if (mlen > 0) {
		oxmlist.unpack((struct ofp_oxm_hdr*)m->oxm_fields, mlen);
	}

	validate();
}



struct ofp13_match*
cofmatch::pack(struct ofp13_match* m, size_t mlen) const throw (eOFmatchInval)
{
	if (mlen < length()) {
		throw eOFmatchInval();
	}

	/*
	 * fill in real length, i.e. excluding padding
	 */
	ofh13_match->length = htobe16(2 * sizeof(uint16_t) + oxmlist.length());

	m->type 	= ofh13_match->type;
	m->length 	= ofh13_match->length;

	oxmlist.pack((struct ofp_oxm_hdr*)(m->oxm_fields), oxmlist.length());

	return m;
}



void
cofmatch::unpack(struct ofp13_match* m, size_t mlen)
{
	of_version = OFP13_VERSION;
	oxmlist.clear();
	memarea.resize(OFP12_MATCH_STATIC_LEN);
	ofh13_match = (struct ofp13_match*)memarea.somem();
	ofh13_match->type 	= htobe16(OFPMT_OXM);
	ofh13_match->length = htobe16(length());

	if (mlen < (sizeof(uint16_t) + sizeof(uint16_t))) {
		throw eOFmatchInval();
	}

	ofh13_match->type	= (m->type);
	ofh13_match->length	= (m->length);

	if (OFPMT_OXM != be16toh(ofh13_match->type)) {
		throw eBadMatchBadType();
	}

	mlen -= 2 * sizeof(uint16_t);

	if (mlen > 0) {
		oxmlist.unpack((struct ofp_oxm_hdr*)m->oxm_fields, mlen);
	}

	validate();
}



bool 
cofmatch::overlaps(
	cofmatch const& m,
	bool strict /* = false (default) */)
{
	return (oxmlist.overlap(m.oxmlist, strict));
}

	

bool 
cofmatch::operator== (
	cofmatch& m)
{
	if (of_version != m.of_version) {
		return false;
	}
	switch (of_version) {
	case OFP10_VERSION: {
		return (oxmlist == m.oxmlist);
	} break;
	case OFP12_VERSION:
	case OFP13_VERSION: {
		if (ofh12_match->type != m.ofh12_match->type) {
			return false;
		}
		return (oxmlist == m.oxmlist);
	} break;
	default:
		throw eBadVersion();
	}
	return false;
}


const char*
cofmatch::c_str()
{
	cvastring vas(3172);

	switch (of_version) {
	case OFP10_VERSION: {
		info.assign(vas("cofmatch(%p) oxmlist.length:%lu oxmlist:%s",
				this,
				oxmlist.length(),
				oxmlist.c_str()));

	} break;
	case OFP12_VERSION:
	case OFP13_VERSION: {
		info.assign(vas("cofmatch(%p) hdr.type:%d hdr.length:%d stored:%lu oxmlist.length:%lu oxmlist:%s",
				this,
				be16toh(ofh12_match->type),
				be16toh(ofh12_match->length),
				length(),
				oxmlist.length(),
				oxmlist.c_str()));
	} break;
	default:
		throw eBadVersion();
	}

	return info.c_str();
}


void
cofmatch::set_type(uint16_t type)
{
	switch (of_version) {
	case OFP12_VERSION:
	case OFP13_VERSION: {
		ofh12_match->type = htobe16(type);
	} break;
	default:
		throw eBadVersion();
	}
}


uint32_t
cofmatch::get_in_port() const throw (eOFmatchNotFound)
{
	if (not oxmlist.exists(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_IN_PORT))
	{
		throw eOFmatchNotFound();
	}

	return oxmlist.get_oxm(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_IN_PORT).u32value();
}


void
cofmatch::set_in_port(uint32_t in_port)
{
	oxmlist[OFPXMT_OFB_IN_PORT] = coxmatch_ofb_in_port(in_port);
}


uint32_t
cofmatch::get_in_phy_port() const throw (eOFmatchNotFound)
{
	switch (of_version) {
	case OFP12_VERSION:
	case OFP13_VERSION: {
		// do nothing
	} break;
	default:
		throw eBadVersion();
	}

	if (not oxmlist.exists(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_IN_PHY_PORT))
	{
		throw eOFmatchNotFound();
	}

	return oxmlist.get_oxm(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_IN_PHY_PORT).u32value();
}


void
cofmatch::set_in_phy_port(uint32_t in_port)
{
	switch (of_version) {
	case OFP12_VERSION:
	case OFP13_VERSION: {
		// do nothing
	} break;
	default:
		throw eBadVersion();
	}

	oxmlist[OFPXMT_OFB_IN_PHY_PORT] = coxmatch_ofb_in_phy_port(in_port);
}


uint64_t
cofmatch::get_metadata() const throw (eOFmatchNotFound)
{
	switch (of_version) {
	case OFP12_VERSION:
	case OFP13_VERSION: {
		// do nothing
	} break;
	default:
		throw eBadVersion();
	}

	if (not oxmlist.exists(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_METADATA))
	{
		throw eOFmatchNotFound();
	}

	return oxmlist.get_oxm(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_METADATA).u64value();
}


void
cofmatch::set_metadata(uint64_t metadata)
{
	switch (of_version) {
	case OFP12_VERSION:
	case OFP13_VERSION: {
		// do nothing
	} break;
	default:
		throw eBadVersion();
	}

	oxmlist[OFPXMT_OFB_METADATA] = coxmatch_ofb_metadata(metadata);
}


cmacaddr
cofmatch::get_eth_dst() const throw (eOFmatchNotFound)
{
	if (not oxmlist.exists(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_ETH_DST))
	{
		throw eOFmatchNotFound();
	}

	return oxmlist.get_oxm(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_ETH_DST).u48addr();
}



cmacaddr
cofmatch::get_eth_dst_addr() const throw (eOFmatchNotFound)
{
	if (not oxmlist.exists(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_ETH_DST))
	{
		throw eOFmatchNotFound();
	}

	return cmacaddr(oxmlist.get_oxm(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_ETH_DST).oxm_maddr->addr, OFP_ETH_ALEN);
}



cmacaddr
cofmatch::get_eth_dst_mask() const throw (eOFmatchNotFound)
{
	if (not oxmlist.exists(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_ETH_DST))
	{
		throw eOFmatchNotFound();
	}

	if (!oxmlist.get_oxm(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_ETH_DST).get_oxm_hasmask()) {
		return cmacaddr("ff:ff:ff:ff:ff:ff");
	}

	return cmacaddr(oxmlist.get_oxm(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_ETH_DST).oxm_maddr->mask, OFP_ETH_ALEN);
}



cmacaddr
cofmatch::get_eth_src() const throw (eOFmatchNotFound)
{
	if (not oxmlist.exists(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_ETH_SRC))
	{
		throw eOFmatchNotFound();
	}

	return oxmlist.get_oxm(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_ETH_SRC).u48addr();
}



cmacaddr
cofmatch::get_eth_src_addr() const throw (eOFmatchNotFound)
{
	if (not oxmlist.exists(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_ETH_SRC))
	{
		throw eOFmatchNotFound();
	}

	return cmacaddr(oxmlist.get_oxm(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_ETH_SRC).oxm_maddr->addr, OFP_ETH_ALEN);
}



cmacaddr
cofmatch::get_eth_src_mask() const throw (eOFmatchNotFound)
{
	if (not oxmlist.exists(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_ETH_SRC))
	{
		throw eOFmatchNotFound();
	}

	if (!oxmlist.get_oxm(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_ETH_SRC).get_oxm_hasmask()) {
		return cmacaddr("ff:ff:ff:ff:ff:ff");
	}

	return cmacaddr(oxmlist.get_oxm(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_ETH_SRC).oxm_maddr->mask, OFP_ETH_ALEN);
}



void
cofmatch::set_eth_dst(cmacaddr const& maddr, cmacaddr const& mmask)
{
	if (cmacaddr("ff:ff:ff:ff:ff:ff") == mmask)
	{
		oxmlist[OFPXMT_OFB_ETH_DST] = coxmatch_ofb_eth_dst(maddr);
	}
	else
	{
		oxmlist[OFPXMT_OFB_ETH_DST] = coxmatch_ofb_eth_dst(maddr, mmask);
	}
}


void
cofmatch::set_eth_src(cmacaddr const& maddr, cmacaddr const& mmask)
{
	if (cmacaddr("ff:ff:ff:ff:ff:ff") == mmask)
	{
		oxmlist[OFPXMT_OFB_ETH_SRC] = coxmatch_ofb_eth_src(maddr);
	}
	else
	{
		oxmlist[OFPXMT_OFB_ETH_SRC] = coxmatch_ofb_eth_src(maddr, mmask);
	}
}


uint16_t
cofmatch::get_eth_type() const throw (eOFmatchNotFound)
{
	if (not oxmlist.exists(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_ETH_TYPE))
	{
		throw eOFmatchNotFound();
	}

	return oxmlist.get_oxm(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_ETH_TYPE).u16value();
}


void
cofmatch::set_eth_type(
		uint16_t dl_type)
{
	oxmlist[OFPXMT_OFB_ETH_TYPE] = coxmatch_ofb_eth_type(dl_type);
}



uint16_t
cofmatch::get_vlan_vid() const throw (eOFmatchNotFound)
{
	if (not oxmlist.exists(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_VLAN_VID))
	{
		throw eOFmatchNotFound();
	}

	return oxmlist.get_oxm(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_VLAN_VID).u16value();
}



uint16_t
cofmatch::get_vlan_vid_value() const throw (eOFmatchNotFound)
{
	if (not oxmlist.exists(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_VLAN_VID))
	{
		throw eOFmatchNotFound();
	}

	return oxmlist.get_oxm(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_VLAN_VID).uint16_value();
}



uint16_t
cofmatch::get_vlan_vid_mask() const throw (eOFmatchNotFound)
{
	if (not oxmlist.exists(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_VLAN_VID))
	{
		throw eOFmatchNotFound();
	}

	return oxmlist.get_oxm(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_VLAN_VID).uint16_mask();
}



void
cofmatch::set_vlan_vid(
		uint16_t vid)
{
	oxmlist[OFPXMT_OFB_VLAN_VID] = coxmatch_ofb_vlan_vid(vid);
}



uint8_t
cofmatch::get_vlan_pcp() const throw (eOFmatchNotFound)
{
	if (not oxmlist.exists(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_VLAN_PCP))
	{
		throw eOFmatchNotFound();
	}

	return oxmlist.get_oxm(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_VLAN_PCP).u8value();
}



void
cofmatch::set_vlan_pcp(
		uint8_t pcp)
{
	oxmlist[OFPXMT_OFB_VLAN_PCP] = coxmatch_ofb_vlan_pcp(pcp);
}



uint32_t
cofmatch::get_mpls_label() const throw (eOFmatchNotFound)
{
	switch (of_version) {
	case OFP12_VERSION:
	case OFP13_VERSION: {
		// do nothing
	} break;
	default:
		throw eBadVersion();
	}

	if (not oxmlist.exists(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_MPLS_LABEL))
	{
		throw eOFmatchNotFound();
	}

	return oxmlist.get_oxm(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_MPLS_LABEL).u32value();
}


void
cofmatch::set_mpls_label(
		uint32_t label)
{
	switch (of_version) {
	case OFP12_VERSION:
	case OFP13_VERSION: {
		// do nothing
	} break;
	default:
		throw eBadVersion();
	}

	oxmlist[OFPXMT_OFB_MPLS_LABEL] = coxmatch_ofb_mpls_label(label);
}


uint8_t
cofmatch::get_mpls_tc() const throw (eOFmatchNotFound)
{
	switch (of_version) {
	case OFP12_VERSION:
	case OFP13_VERSION: {
		// do nothing
	} break;
	default:
		throw eBadVersion();
	}

	if (not oxmlist.exists(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_MPLS_TC))
	{
		throw eOFmatchNotFound();
	}

	return oxmlist.get_oxm(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_MPLS_TC).u8value();
}


void
cofmatch::set_mpls_tc(
		uint8_t tc)
{
	switch (of_version) {
	case OFP12_VERSION:
	case OFP13_VERSION: {
		// do nothing
	} break;
	default:
		throw eBadVersion();
	}

	oxmlist[OFPXMT_OFB_MPLS_TC] = coxmatch_ofb_mpls_tc(tc);
}



uint8_t
cofmatch::get_pppoe_type() const throw (eOFmatchNotFound)
{
	if (not oxmlist.exists(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFX_PPPOE_TYPE))
	{
		throw eOFmatchNotFound();
	}

	return oxmlist.get_oxm(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFX_PPPOE_TYPE).u8value();
}



void
cofmatch::set_pppoe_type(
		uint8_t type)
{
	oxmlist[OFPXMT_OFX_PPPOE_TYPE] = coxmatch_ofx_pppoe_type(type);
}



uint8_t
cofmatch::get_pppoe_code() const throw (eOFmatchNotFound)
{
	if (not oxmlist.exists(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFX_PPPOE_CODE))
	{
		throw eOFmatchNotFound();
	}

	return oxmlist.get_oxm(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFX_PPPOE_CODE).u8value();
}


void
cofmatch::set_pppoe_code(
		uint8_t code)
{
	oxmlist[OFPXMT_OFX_PPPOE_CODE] = coxmatch_ofx_pppoe_code(code);
}


uint16_t
cofmatch::get_pppoe_sessid() const throw (eOFmatchNotFound)
{
	if (not oxmlist.exists(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFX_PPPOE_SID))
	{
		throw eOFmatchNotFound();
	}

	return oxmlist.get_oxm(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFX_PPPOE_SID).u16value();
}


void
cofmatch::set_pppoe_sessid(
		uint16_t sid)
{
	oxmlist[OFPXMT_OFX_PPPOE_SID] = coxmatch_ofx_pppoe_sid(sid);
}


uint16_t
cofmatch::get_ppp_prot() const throw (eOFmatchNotFound)
{
	if (not oxmlist.exists(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFX_PPP_PROT))
	{
		throw eOFmatchNotFound();
	}

	return oxmlist.get_oxm(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFX_PPP_PROT).u16value();
}


void
cofmatch::set_ppp_prot(
		uint16_t prot)
{
	oxmlist[OFPXMT_OFX_PPP_PROT] = coxmatch_ofx_ppp_prot(prot);
}


caddress
cofmatch::get_ipv4_src() const throw (eOFmatchNotFound)
{
	if (not oxmlist.exists(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_IPV4_SRC))
	{
		throw eOFmatchNotFound();
	}
	return oxmlist.get_oxm(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_IPV4_SRC).u32addr();
}



caddress
cofmatch::get_ipv4_src_value() const throw (eOFmatchNotFound)
{
	if (not oxmlist.exists(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_IPV4_SRC))
	{
		throw eOFmatchNotFound();
	}

	caddress addr(AF_INET, "0.0.0.0");
	addr.ca_s4addr->sin_addr.s_addr =
			htobe32(oxmlist.get_oxm(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_IPV4_SRC).uint32_value());
	return addr;
}



caddress
cofmatch::get_ipv4_src_mask() const throw (eOFmatchNotFound)
{
	if (not oxmlist.exists(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_IPV4_SRC))
	{
		throw eOFmatchNotFound();
	}

	caddress mask(AF_INET, "0.0.0.0");
	mask.ca_s4addr->sin_addr.s_addr =
			htobe32(oxmlist.get_oxm(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_IPV4_SRC).uint32_mask());
	return mask;
}



void
cofmatch::set_ipv4_src(
		caddress const& src)
{
	oxmlist[OFPXMT_OFB_IPV4_SRC] = coxmatch_ofb_ipv4_src(src);
}


void
cofmatch::set_ipv4_src(
		caddress const& src,
		caddress const& mask)
{
	oxmlist[OFPXMT_OFB_IPV4_SRC] = coxmatch_ofb_ipv4_src(src, mask);
}



caddress
cofmatch::get_ipv4_dst() const throw (eOFmatchNotFound)
{
	if (not oxmlist.exists(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_IPV4_DST))
	{
		throw eOFmatchNotFound();
	}
	return oxmlist.get_oxm(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_IPV4_DST).u32addr();
}



caddress
cofmatch::get_ipv4_dst_value() const throw (eOFmatchNotFound)
{
	if (not oxmlist.exists(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_IPV4_DST))
	{
		throw eOFmatchNotFound();
	}

	caddress addr(AF_INET, "0.0.0.0");
	addr.ca_s4addr->sin_addr.s_addr =
			htobe32(oxmlist.get_oxm(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_IPV4_DST).uint32_value());
	return addr;
}



caddress
cofmatch::get_ipv4_dst_mask() const throw (eOFmatchNotFound)
{
	if (not oxmlist.exists(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_IPV4_DST))
	{
		throw eOFmatchNotFound();
	}

	caddress mask(AF_INET, "0.0.0.0");
	mask.ca_s4addr->sin_addr.s_addr =
			htobe32(oxmlist.get_oxm(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_IPV4_DST).uint32_mask());
	return mask;
}



void
cofmatch::set_ipv4_dst(
		caddress const& dst)
{
	oxmlist[OFPXMT_OFB_IPV4_DST] = coxmatch_ofb_ipv4_dst(dst);
}


void
cofmatch::set_ipv4_dst(
		caddress const& dst,
		caddress const& mask)
{
	oxmlist[OFPXMT_OFB_IPV4_DST] = coxmatch_ofb_ipv4_dst(dst, mask);
}



uint16_t
cofmatch::get_arp_opcode() const throw (eOFmatchNotFound)
{
	if (not oxmlist.exists(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_ARP_OP))
	{
		throw eOFmatchNotFound();
	}
	return oxmlist.get_oxm(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_ARP_OP).u16value();
}



void
cofmatch::set_arp_opcode(
		uint16_t opcode)
{
	oxmlist[OFPXMT_OFB_ARP_OP] = coxmatch_ofb_arp_op(opcode);
}



cmacaddr
cofmatch::get_arp_sha() const throw (eOFmatchNotFound)
{
	if (not oxmlist.exists(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_ARP_SHA))
	{
		throw eOFmatchNotFound();
	}
	return oxmlist.get_oxm(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_ARP_SHA).u48addr();
}



void
cofmatch::set_arp_sha(
		cmacaddr const& sha)
{
	oxmlist[OFPXMT_OFB_ARP_SHA] = coxmatch_ofb_arp_sha(sha);
}



cmacaddr
cofmatch::get_arp_tha() const throw (eOFmatchNotFound)
{
	if (not oxmlist.exists(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_ARP_THA))
	{
		throw eOFmatchNotFound();
	}
	return oxmlist.get_oxm(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_ARP_THA).u48addr();
}



void
cofmatch::set_arp_tha(
		cmacaddr const& tha)
{
	oxmlist[OFPXMT_OFB_ARP_THA] = coxmatch_ofb_arp_tha(tha);
}



caddress
cofmatch::get_arp_spa() const throw (eOFmatchNotFound)
{
	if (not oxmlist.exists(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_ARP_SPA))
	{
		throw eOFmatchNotFound();
	}
	return oxmlist.get_oxm(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_ARP_SPA).u32addr();
}



void
cofmatch::set_arp_spa(
		caddress const& spa)
{
	oxmlist[OFPXMT_OFB_ARP_SPA] = coxmatch_ofb_arp_spa(spa);
}



caddress
cofmatch::get_arp_tpa() const throw (eOFmatchNotFound)
{
	if (not oxmlist.exists(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_ARP_TPA))
	{
		throw eOFmatchNotFound();
	}
	return oxmlist.get_oxm(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_ARP_TPA).u32addr();
}



void
cofmatch::set_arp_tpa(
		caddress const& tpa)
{
	oxmlist[OFPXMT_OFB_ARP_TPA] = coxmatch_ofb_arp_tpa(tpa);
}




caddress
cofmatch::get_ipv6_src() const throw (eOFmatchNotFound)
{
	switch (of_version) {
	case OFP12_VERSION:
	case OFP13_VERSION: {
		// do nothing
	} break;
	default:
		throw eBadVersion();
	}

	if (not oxmlist.exists(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_IPV6_SRC))
	{
		throw eOFmatchNotFound();
	}

	return oxmlist.get_oxm(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_IPV6_SRC).u128addr();
}



void
cofmatch::set_ipv6_src(
		caddress const& addr)
{
	switch (of_version) {
	case OFP12_VERSION:
	case OFP13_VERSION: {
		// do nothing
	} break;
	default:
		throw eBadVersion();
	}

	oxmlist[OFPXMT_OFB_IPV6_SRC] = coxmatch_ofb_ipv6_src(addr);
}



void
cofmatch::set_ipv6_src(
		caddress const& addr,
		caddress const& mask)
{
	switch (of_version) {
	case OFP12_VERSION:
	case OFP13_VERSION: {
		// do nothing
	} break;
	default:
		throw eBadVersion();
	}

	oxmlist[OFPXMT_OFB_IPV6_SRC] = coxmatch_ofb_ipv6_src(addr, mask);
}



caddress
cofmatch::get_ipv6_dst() const throw (eOFmatchNotFound)
{
	if (not oxmlist.exists(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_IPV6_DST))
	{
		throw eOFmatchNotFound();
	}
	return oxmlist.get_oxm(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_IPV6_DST).u128addr();
}



void
cofmatch::set_ipv6_dst(
		caddress const& addr)
{
	switch (of_version) {
	case OFP12_VERSION:
	case OFP13_VERSION: {
		// do nothing
	} break;
	default:
		throw eBadVersion();
	}

	oxmlist[OFPXMT_OFB_IPV6_DST] = coxmatch_ofb_ipv6_dst(addr);
}



void
cofmatch::set_ipv6_dst(
		caddress const& addr,
		caddress const& mask)
{
	switch (of_version) {
	case OFP12_VERSION:
	case OFP13_VERSION: {
		// do nothing
	} break;
	default:
		throw eBadVersion();
	}

	oxmlist[OFPXMT_OFB_IPV6_DST] = coxmatch_ofb_ipv6_dst(addr, mask);
}



caddress
cofmatch::get_ipv6_nd_target() const throw (eOFmatchNotFound)
{
	switch (of_version) {
	case OFP12_VERSION:
	case OFP13_VERSION: {
		// do nothing
	} break;
	default:
		throw eBadVersion();
	}

	if (not oxmlist.exists(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_IPV6_ND_TARGET))
	{
		throw eOFmatchNotFound();
	}
	return oxmlist.get_oxm(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_IPV6_ND_TARGET).u128addr();
}



void
cofmatch::set_ipv6_nd_target(
		caddress const& addr)
{
	switch (of_version) {
	case OFP12_VERSION:
	case OFP13_VERSION: {
		// do nothing
	} break;
	default:
		throw eBadVersion();
	}

	oxmlist[OFPXMT_OFB_IPV6_ND_TARGET] = coxmatch_ofb_ipv6_nd_target(addr);
}



uint8_t
cofmatch::get_ip_proto() const throw (eOFmatchNotFound)
{
	if (not oxmlist.exists(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_IP_PROTO))
	{
		throw eOFmatchNotFound();
	}

	return oxmlist.get_oxm(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_IP_PROTO).u8value();
}



void
cofmatch::set_ip_proto(
		uint8_t proto)
{
	oxmlist[OFPXMT_OFB_IP_PROTO] = coxmatch_ofb_ip_proto(proto);
}



uint8_t
cofmatch::get_ip_dscp() const throw (eOFmatchNotFound)
{
	if (not oxmlist.exists(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_IP_DSCP))
	{
		throw eOFmatchNotFound();
	}

	return oxmlist.get_oxm(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_IP_DSCP).u8value();
}



void
cofmatch::set_ip_dscp(
		uint8_t dscp)
{
	oxmlist[OFPXMT_OFB_IP_DSCP] = coxmatch_ofb_ip_dscp(dscp);
}



uint8_t
cofmatch::get_ip_ecn() const throw (eOFmatchNotFound)
{
	switch (of_version) {
	case OFP12_VERSION:
	case OFP13_VERSION: {
		// do nothing
	} break;
	default:
		throw eBadVersion();
	}

	if (not oxmlist.exists(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_IP_ECN))
	{
		throw eOFmatchNotFound();
	}

	return oxmlist.get_oxm(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_IP_ECN).u8value();
}



void
cofmatch::set_ip_ecn(
		uint8_t ecn)
{
	switch (of_version) {
	case OFP12_VERSION:
	case OFP13_VERSION: {
		// do nothing
	} break;
	default:
		throw eBadVersion();
	}

	oxmlist[OFPXMT_OFB_IP_ECN] = coxmatch_ofb_ip_ecn(ecn);
}



uint8_t
cofmatch::get_icmpv4_type() const throw (eOFmatchNotFound)
{
	if (not oxmlist.exists(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_ICMPV4_TYPE))
	{
		throw eOFmatchNotFound();
	}

	return oxmlist.get_oxm(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_ICMPV4_TYPE).u8value();
}



void
cofmatch::set_icmpv4_type(
		uint8_t type)
{
	oxmlist[OFPXMT_OFB_ICMPV4_TYPE] = coxmatch_ofb_icmpv4_type(type);
}



uint8_t
cofmatch::get_icmpv4_code() const throw (eOFmatchNotFound)
{
	if (not oxmlist.exists(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_ICMPV4_CODE))
	{
		throw eOFmatchNotFound();
	}

	return oxmlist.get_oxm(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_ICMPV4_CODE).u8value();
}



void
cofmatch::set_icmpv4_code(
		uint8_t code)
{
	oxmlist[OFPXMT_OFB_ICMPV4_CODE] = coxmatch_ofb_icmpv4_code(code);
}



uint8_t
cofmatch::get_icmpv6_type() const throw (eOFmatchNotFound)
{
	switch (of_version) {
	case OFP12_VERSION:
	case OFP13_VERSION: {
		// do nothing
	} break;
	default:
		throw eBadVersion();
	}

	if (not oxmlist.exists(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_ICMPV6_TYPE))
	{
		throw eOFmatchNotFound();
	}

	return oxmlist.get_oxm(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_ICMPV6_TYPE).u8value();
}



void
cofmatch::set_icmpv6_type(
		uint8_t type)
{
	switch (of_version) {
	case OFP12_VERSION:
	case OFP13_VERSION: {
		// do nothing
	} break;
	default:
		throw eBadVersion();
	}

	oxmlist[OFPXMT_OFB_ICMPV6_TYPE] = coxmatch_ofb_icmpv6_type(type);
}



uint8_t
cofmatch::get_icmpv6_code() const throw (eOFmatchNotFound)
{
	switch (of_version) {
	case OFP12_VERSION:
	case OFP13_VERSION: {
		// do nothing
	} break;
	default:
		throw eBadVersion();
	}

	if (not oxmlist.exists(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_ICMPV6_CODE))
	{
		throw eOFmatchNotFound();
	}

	return oxmlist.get_oxm(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_ICMPV6_CODE).u8value();
}



void
cofmatch::set_icmpv6_code(
		uint8_t code)
{
	switch (of_version) {
	case OFP12_VERSION:
	case OFP13_VERSION: {
		// do nothing
	} break;
	default:
		throw eBadVersion();
	}

	oxmlist[OFPXMT_OFB_ICMPV6_CODE] = coxmatch_ofb_icmpv6_code(code);
}



uint32_t
cofmatch::get_ipv6_flabel() const throw (eOFmatchNotFound)
{
	switch (of_version) {
	case OFP12_VERSION:
	case OFP13_VERSION: {
		// do nothing
	} break;
	default:
		throw eBadVersion();
	}

	if (not oxmlist.exists(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_IPV6_FLABEL))
	{
		throw eOFmatchNotFound();
	}

	return oxmlist.get_oxm(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_IPV6_FLABEL).u32value();
}



void
cofmatch::set_ipv6_flabel(
		uint32_t flabel)
{
	switch (of_version) {
	case OFP12_VERSION:
	case OFP13_VERSION: {
		// do nothing
	} break;
	default:
		throw eBadVersion();
	}

	oxmlist[OFPXMT_OFB_IPV6_FLABEL] = coxmatch_ofb_ipv6_flabel(flabel);
}



void
cofmatch::set_ipv6_flabel(
		uint32_t flabel,
		uint32_t mask)
{
	switch (of_version) {
	case OFP12_VERSION:
	case OFP13_VERSION: {
		// do nothing
	} break;
	default:
		throw eBadVersion();
	}

	oxmlist[OFPXMT_OFB_IPV6_FLABEL] = coxmatch_ofb_ipv6_flabel(flabel, mask);
}



cmacaddr
cofmatch::get_icmpv6_neighbor_source_lladdr() const throw (eOFmatchNotFound)
{
	switch (of_version) {
	case OFP12_VERSION:
	case OFP13_VERSION: {
		// do nothing
	} break;
	default:
		throw eBadVersion();
	}

	if (not oxmlist.exists(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_IPV6_ND_SLL))
	{
		throw eOFmatchNotFound();
	}
	return oxmlist.get_oxm(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_IPV6_ND_SLL).u48addr();
}



void
cofmatch::set_icmpv6_neighbor_source_lladdr(
		cmacaddr const& maddr)
{
	switch (of_version) {
	case OFP12_VERSION:
	case OFP13_VERSION: {
		// do nothing
	} break;
	default:
		throw eBadVersion();
	}

	oxmlist[OFPXMT_OFB_IPV6_ND_SLL] = coxmatch_ofb_ipv6_nd_sll(maddr);
}



cmacaddr
cofmatch::get_icmpv6_neighbor_target_lladdr() const throw (eOFmatchNotFound)
{
	switch (of_version) {
	case OFP12_VERSION:
	case OFP13_VERSION: {
		// do nothing
	} break;
	default:
		throw eBadVersion();
	}

	if (not oxmlist.exists(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_IPV6_ND_TLL))
	{
		throw eOFmatchNotFound();
	}
	return oxmlist.get_oxm(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_IPV6_ND_TLL).u48addr();
}



void
cofmatch::set_icmpv6_neighbor_target_lladdr(
		cmacaddr const& maddr)
{
	switch (of_version) {
	case OFP12_VERSION:
	case OFP13_VERSION: {
		// do nothing
	} break;
	default:
		throw eBadVersion();
	}

	oxmlist[OFPXMT_OFB_IPV6_ND_TLL] = coxmatch_ofb_ipv6_nd_tll(maddr);
}



caddress
cofmatch::get_icmpv6_neighbor_taddr() const throw (eOFmatchNotFound)
{
	switch (of_version) {
	case OFP12_VERSION:
	case OFP13_VERSION: {
		// do nothing
	} break;
	default:
		throw eBadVersion();
	}

	if (not oxmlist.exists(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_IPV6_ND_TARGET))
	{
		throw eOFmatchNotFound();
	}
	return oxmlist.get_oxm(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_IPV6_ND_TARGET).u128addr();
}



void
cofmatch::set_icmpv6_neighbor_taddr(
		caddress const& addr)
{
	switch (of_version) {
	case OFP12_VERSION:
	case OFP13_VERSION: {
		// do nothing
	} break;
	default:
		throw eBadVersion();
	}

	oxmlist[OFPXMT_OFB_IPV6_ND_TARGET] = coxmatch_ofb_ipv6_nd_target(addr);
}



uint16_t
cofmatch::get_udp_src() const throw (eOFmatchNotFound)
{
	if (not oxmlist.exists(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_UDP_SRC))
	{
		throw eOFmatchNotFound();
	}
	return oxmlist.get_oxm(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_UDP_SRC).u16value();
}


void
cofmatch::set_udp_src(
		uint16_t src_port)
{
	oxmlist[OFPXMT_OFB_UDP_SRC] = coxmatch_ofb_udp_src(src_port);
}


uint16_t
cofmatch::get_udp_dst() const throw (eOFmatchNotFound)
{
	if (not oxmlist.exists(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_UDP_DST))
	{
		throw eOFmatchNotFound();
	}
	return oxmlist.get_oxm(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_UDP_DST).u16value();
}


void
cofmatch::set_udp_dst(
		uint16_t dst_port)
{
	oxmlist[OFPXMT_OFB_UDP_DST] = coxmatch_ofb_udp_dst(dst_port);
}



uint16_t
cofmatch::get_tcp_src() const throw (eOFmatchNotFound)
{
	if (not oxmlist.exists(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_TCP_SRC))
	{
		throw eOFmatchNotFound();
	}
	return oxmlist.get_oxm(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_TCP_SRC).u16value();
}



void
cofmatch::set_tcp_src(
		uint16_t src_port)
{
	oxmlist[OFPXMT_OFB_TCP_SRC] = coxmatch_ofb_tcp_src(src_port);
}



uint16_t
cofmatch::get_tcp_dst() const throw (eOFmatchNotFound)
{
	if (not oxmlist.exists(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_TCP_DST))
	{
		throw eOFmatchNotFound();
	}
	return oxmlist.get_oxm(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_TCP_DST).u16value();
}



void
cofmatch::set_tcp_dst(
		uint16_t dst_port)
{
	oxmlist[OFPXMT_OFB_TCP_DST] = coxmatch_ofb_tcp_dst(dst_port);
}




uint16_t
cofmatch::get_sctp_src() const throw (eOFmatchNotFound)
{
	switch (of_version) {
	case OFP12_VERSION:
	case OFP13_VERSION: {
		// do nothing
	} break;
	default:
		throw eBadVersion();
	}

	if (not oxmlist.exists(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_SCTP_SRC))
	{
		throw eOFmatchNotFound();
	}
	return oxmlist.get_oxm(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_SCTP_SRC).u16value();
}



void
cofmatch::set_sctp_src(
		uint16_t src_port)
{
	switch (of_version) {
	case OFP12_VERSION:
	case OFP13_VERSION: {
		// do nothing
	} break;
	default:
		throw eBadVersion();
	}

	oxmlist[OFPXMT_OFB_SCTP_SRC] = coxmatch_ofb_sctp_src(src_port);
}



uint16_t
cofmatch::get_sctp_dst() const throw (eOFmatchNotFound)
{
	switch (of_version) {
	case OFP12_VERSION:
	case OFP13_VERSION: {
		// do nothing
	} break;
	default:
		throw eBadVersion();
	}

	if (not oxmlist.exists(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_SCTP_DST))
	{
		throw eOFmatchNotFound();
	}
	return oxmlist.get_oxm(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_SCTP_DST).u16value();
}



void
cofmatch::set_sctp_dst(
		uint16_t dst_port)
{
	switch (of_version) {
	case OFP12_VERSION:
	case OFP13_VERSION: {
		// do nothing
	} break;
	default:
		throw eBadVersion();
	}

	oxmlist[OFPXMT_OFB_SCTP_DST] = coxmatch_ofb_sctp_dst(dst_port);
}


#ifndef NDEBUG
void
cofmatch::test()
{
	cofmatch m;

	m.oxmlist[OFPXMT_OFB_ETH_SRC] 	= coxmatch_ofb_eth_src(cmacaddr("11:11:11:11:11:11"), cmacaddr("33:33:33:33:33:33"));
	m.oxmlist[OFPXMT_OFB_VLAN_VID] 	= coxmatch_ofb_vlan_vid(1000);
	m.oxmlist[OFPXMT_OFB_IP_DSCP] 	= coxmatch_ofb_ip_dscp(6);

	cmemory mem(m.length());

	m.pack((struct ofp12_match*)mem.somem(), mem.memlen());
	fprintf(stderr, "match: %s\nmem:%s\n\n", m.c_str(), mem.c_str());

	cofmatch tm(m);

	fprintf(stderr, "tm: %s\n\n", tm.c_str());

	cofmatch cm;

	cm.unpack((struct ofp12_match*)mem.somem(), mem.memlen());
	fprintf(stderr, "unpack: %s\n\n", cm.c_str());

	{
		cofmatch m;

		m.set_in_port(47);
		m.set_in_phy_port(47);
		m.set_eth_dst(cmacaddr("11:11:11:11:11:11"));
		m.set_eth_src(cmacaddr("22:22:22:22:22:22"));

		fprintf(stderr, "cofmatch: %s\n\n", m.c_str());
	}
}
#endif

